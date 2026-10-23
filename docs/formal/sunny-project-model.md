# Sunny cross-IR project model

## Status and scope

This document specifies the aggregate model that binds Score, Timbre, and Mix IR documents to one
production target. The individual IR specifications remain authoritative for their local domains.
This model is authoritative for cross-document identity, validation order, target allocation, and
aggregate compilation.

Corpus IR is deliberately not a deployment component. It may inform authoring, but it does not
describe state that must be materialised in the target Live Set.

## 1. Project view

A project view is the tuple

\[
J = (S, T, M)
\]

where:

- \(S\) is one Score;
- \(T\) is a finite collection of non-null TimbreProfile references;
- \(M\) is one MixGraph.

The runtime representation is non-owning. MCP keeps the documents in identity-stable shared
session stores and constructs a `ProjectView` for each request. This prevents aggregate validation
from copying move-only timbre structures or inventing a second identity namespace.

### 1.1 Domain closure and assumptions

The aggregate model is complete only relative to its declared source and target domains:

- Score owns symbolic pitch/spelling, exact musical time, note/notation structure, expression,
  articulation-to-performance mappings, and the complete `[0,127]` source tuning function.
- Each bound TimbreProfile owns requested sound sources, devices/effects, modulation, and explicit
  source-to-target parameter mappings for one Part.
- MixGraph owns channel/aux/master topology, levels, pan, sends, effects, automation intent, and
  output-destination intent.
- Corpus is evidence for authoring and owns no deployment state.
- The Live target owns ambient state outside the admitted mutation algebra. Snapshot and final
  evidence make the tractable subset observable; unsupported or under-specified properties remain
  requested/written/verified residuals. No structural `complete` verdict implies waveform,
  playback-trace, hardware-output, or acoustic equivalence.

Thus no individual IR is “supplementary” to another: the tuple is the production source model, and
the exact `PartId` correspondence below is the composition law. A project is locally specified when
all three documents validate and the bijections hold. It is externally realized only to the extent
reported by the selected target compiler and its evidence.

### 1.2 Owning authoring bindings

`McpSession` owns the canonical Score, Timbre, and Mix stores. `bind_project` joins one existing
`score_id`, an exact collection of existing `profile_id` values, and one existing `mix_graph_id`.
The Score identity is also the authoring-project identity; no parallel ID namespace or second live
copy of a component is introduced. A Score, profile, or graph belongs to at most one authoring
binding. Local `PartId` equality between different Scores never implies component ownership.
`create_project(score_id)` constructs default sibling profiles and channels for an existing Score
and establishes the binding as one request. Unbound scratch documents retain their independent
tools and Score-only history.

Every tool in a Score/Timbre/Mix registration group defaults to authoring-transaction routing;
the explicitly declared pure queries and exports opt out. The bound owner is selected from the
tool's existing document ID, so the original direct mutation tools and `score_undo`/`score_redo`
use the same complete project history. Live target tools are outside this local rollback boundary.
Binding starts a new history and discards incompatible pre-binding Score-only snapshots.

One successful changed request records immutable complete before/after component values, with the
newest 64 entries retained by default. Any new sibling edit clears redo. Undo/redo restores authored
Score/Timbre/Mix state and original typed identities together while increasing the Score version
and project revision. Score's typed identity reservations and observed channel reservations survive
undo, removal, and redo; they reserve observed identities, preserving unused holes beneath imported
high IDs. Session-wide profile/effect/preset/bus counters never rewind during history traversal.

`score_add_part` adds its default Timbre profile and ChannelStrip in the same transaction.
`score_remove_part` removes its owned siblings and their reverse group-membership edges; retained
AudioFollower, sidechain, automation, relative-fader, and processing-rationale references must be
explicitly repaired first. `score_reorder_parts` preserves identities and follows author order in
the bound sibling collections. A rejection restores component values, reservations, counters, and
history. Shared-preset history changes only the library identities touched by that edit, preserving
independent presets from other projects; incoming retained morph references prevent their removal.

Workspace save/open/import and explicit backup recovery persist the complete authored bindings and
components under the [workspace contract](sunny-workspace-format.md). External asset collection
remains outside that file format.

Whole-measure insertion and deletion relocate bar-anchored Timbre and Mix controls inside the same
transaction. Insertion shifts every later anchor by the inserted count. Deletion removes lane
points inside the removed bars, shifts later points, and removes empty lanes. Morph endpoints inside
the removed bars clip to the surviving join downbeat; collapsed morphs are removed. Responses report
the affected counts. Existing interpolation follows the relocated anchors. Meter changes preserve
bar anchors and are rejected atomically when an offset no longer fits its owning Score.

## 2. Identity and correspondence

Let \(P(S)\) be the ordered sequence of Parts in the Score. Let \(p(x)\) denote the `PartId` carried
by a Part, TimbreProfile, or ChannelStrip.

The following conditions are necessary for project compilation:

1. Part IDs in \(P(S)\) are unique under Score validation.
2. For every Part \(s \in P(S)\), exactly one \(t \in T\) satisfies \(p(t)=p(s)\).
3. Every \(t \in T\) refers to a Part in \(P(S)\); no orphan profile is permitted.
4. For every Part \(s \in P(S)\), exactly one ChannelStrip \(c \in M.channels\) satisfies
   \(p(c)=p(s)\).
5. Every ChannelStrip refers to a Part in \(P(S)\); no orphan channel is permitted.

Thus the `PartId` projections from Timbre profiles and channels to Score Parts are bijections. The
correspondence relation is by typed identity, never by name, MIDI channel, collection position, or
instrument category.

T1 reports missing, duplicate, and unknown Timbre bindings. X1 reports missing, duplicate, and
unknown Mix bindings. Error codes 6000–6003 and 7000/7006/7007 identify these cases.
P2 reports a primary or via Timbre `AudioFollower`, an active recursive Hybrid crossfade
`AudioFollower`, or a Timbre compressor sidechain whose referenced `PartId` is absent from the
Score, including references in bypassed compressor effects. Local Timbre modulation validation
separately rejects zero or self-referential primary/via AudioFollower payloads. Presets store
numeric parameter states rather than source/effect structures and add no typed Part edges.

## 3. Authoritative target allocation

For a validated ordered Score Part sequence

\[
P(S) = [p_0, p_1, \ldots, p_{n-1}],
\]

aggregate Ableton compilation defines

\[
\phi(p_i) = i.
\]

`φ` is the sole track-allocation function for downstream project compilation:

- Score creates the MIDI track for `p_i` at Live track index `i`;
- the unique TimbreProfile bound to `p_i` inserts devices on track `φ(p_i)`;
- the unique ChannelStrip bound to `p_i` configures mixer state on track `φ(p_i)`.

Reordering `T` or `M.channels` cannot alter target selection. This property is tested
adversarially. The Project tools are the only MCP path that writes Score, Timbre, or Mix state to
Live; no per-document compile tool or position-based track overload remains.

## 4. Validation function

`validate_project(J)` evaluates, in deterministic source order:

1. all Score validation rules;
2. all local Timbre validation rules for every supplied profile;
3. T1 correspondence;
4. P2 cross-Part AudioFollower references;
5. all local Mix validation rules;
6. X1 correspondence;
7. P4 exact temporal closure of all Timbre automation, preset morph endpoints, and Mix automation.
   Controls occupy a measured bar with an in-meter rational offset, or the final downbeat
   `(total_bars + 1, 0)`. They cannot refer to absent bars or the interior of the final endpoint.

Diagnostics are stable-sorted by severity. `is_project_compilable(J)` holds iff the result contains
no Error diagnostic. Warning and Info diagnostics remain visible but do not block compilation.

The compiler performs this complete validation before the first transport mutation. Invalid
projects return error 9000 (`ProjectValidationFailed`). Error 9001
(`ProjectMissingComponent`) is a defensive failure for a component that disappears after
successful preflight; stable MCP session ownership makes that case unreachable during a normal
synchronous request. Plan/apply errors are 9002 (`ProjectPlanProjectChanged`), 9003
(`ProjectPlanTargetChanged`), 9004 (`ProjectPlanDiverged`), 9005
(`TargetSnapshotUnavailable`), and 9006 (`ProjectPlanConsumed`).

## 5. Guarded plan/apply compilation

For a compilable project \(J\), planning constructs

\[
\Pi = (C(J),\; \tau,\; \phi,\; q,\; [m_0,\ldots,m_{k-1}]),
\]

where \(C(J)\) is the canonical lossless JSON state of all three IRs (with Timbre profiles sorted
by typed identity), \(\tau\) is one synchronous main-thread target observation, \(\phi\) is the Part allocation,
\(q\) is PPQ, and each \(m_i\) is an exact phase/type/path/name/argument mutation.
`plan_project_to_ableton`:

1. validates all local/cross-IR rules and PPQ;
2. reads \(\tau\) without mutating Live;
3. seeds an isolated recording transport from \(\tau\)'s return/device topology;
4. dry-runs Score structure/content, Timbre in Score Part order through \(\phi\), and Mix through
   the same \(\phi\);
5. retains the preview and exact ordered mutations.

Application marks the move-only plan object consumed, then requires \(C(J')=C(J)\) and a fresh
target observation \(\tau'=\tau\). A mismatch emits no mutation, but the attempted application does
not make a stale plan reusable. Reapplying the same C++ plan object or MCP plan identifier returns
9006 (`ProjectPlanConsumed`) before target access. Before each request, its phase and complete wire
JSON must equal the next \(m_i\); divergence is declined locally. Every attempted mutation produces
a journal entry classified as `recorded_only`, `acknowledged`, `declined_before_send`, or
`indeterminate`. The transport separately proves `not_sent`, `sent_without_valid_response`, or
`response_received`; a local size/connectivity/plan decline is therefore not falsely classified as
a possible Live mutation. A frame that never completed is `not_sent`: the peer dispatches only
complete frames, and the transport abandons the connection. Once a complete request frame may have
been sent, failure is indeterminate rather than “rejected” because a setter/call may have mutated
Live before readback, acknowledgement, peer failure, or a socket timeout. A post-attempt observation is requested even
after compilation failure when the connection remains usable.

The successful compilation result retains the explicit Part-to-track map, local compiler results,
target profile, diagnostics, parameter/property evidence, the complete Mix fader-resolution proof,
capability warnings, requested/observed/verified post-apply Song/Scene/Cue state, Part Track gates,
generated Session Clip state, and inserted-device chain records, and separate
requested/written automation-lane counts for each Timbre result and the Mix result. `success` means
every planned command was issued in order, accepted, and
all current-protocol evidence required from a real transport was well formed. `complete` means no
requested semantic property was left unapplied or observably divergent. These are not synonyms,
and neither establishes audible equivalence.
The Score sub-result includes its complete MIDI compilation report; any dropped event or tuning
residual participates in aggregate MCP `complete` rather than disappearing behind otherwise valid
Timbre/Mix results.

Every typed deployment observation in the Score, Timbre, and Mix sub-results is encoded by one
shared Infrastructure JSON projection (`mcp/evidence_encoding`). This includes Score tuning, notes, Clip-envelope
clears, CuePoints, scalar properties, Timbre parameters, inserted devices, Mix Return/Main Track
intent, Mix parameters, and Mix parameter coverage. Therefore aggregation changes containment but
not the field inventory or interpretation of the observation. This is an internal schema invariant;
it does not establish that Live or Max produced, persisted, scheduled, rendered, or sounded the
reported state.

`project_validate` is target-free. `project_plan_to_ableton` is target-read-only;
`project_apply_ableton_plan` consumes both its move-only C++ plan object and stored MCP plan once.
An MCP attempt made while its transport is unavailable does not enter the C++ apply operation and
therefore leaves the plan unconsumed. `project_compile_to_ableton` is an
immediate plan/apply convenience and therefore remains repeatable only in the operational sense:
calling it again constructs a new creation plan. `score_create` returns allocated `part_ids`, so
clients need not guess process-global identifiers.

## 6. Ableton/Max external contract

The aggregate model assumes only the target operations admitted by the bridge contract
(`remote_script/Sunny/bridge_contract.json`). In particular:

- Score note insertion is available only when the observed target profile permits the documented
  `Clip.add_new_notes` surface;
- the initial Score meter must fit Live's documented 1–99 numerator and
  {1, 2, 4, 8, 16} denominator subset before any target access, and its values are then
  independently set/read back on Song and every created Session Clip; Scene 0 tempo and meter
  overrides are explicitly disabled/read back so launch uses Song state, and its name is set/read
  back from the full Score title;
- every generated Session Clip is set/read back as active and as a non-looping interval from beat
  zero through the compiled Score end. On Live 11+, the target projection additionally owns the
  closed launch tuple Trigger/None/Legato-off/velocity-amount-zero; this is adapter state rather
  than a general Score launch model. Every generated Clip also receives the public all-envelope
  clear with immediate Boolean absence evidence before note insertion;
- native device insertion is available only for documented Live versions exposing
  `Track.insert_device`, currently modelled as Live 12.3+;
- return allocation begins at the observed existing `Song.return_tracks` count;
- every Sunny device count, snapshot list, and `devices/N` path uses the same mixer-excluding
  insertable-chain index domain rather than the raw documented `Track.devices` list;
- current-protocol requests and responses carry exact versioned envelopes; every nested target-evidence
  object is closed as well, including the three-member generic property, thirteen-member named
  DeviceParameter setter records, and twelve-member read-only parameter observations, including
  the conditional continuous-default or quantized-label domain, so an unknown
  response member fails rather than silently widening the
  model; canonical `song/...` paths, the allowlisted operation family, and checked argument domains
  are validated before Live object traversal, so the bridge cannot be used as an unmodelled
  reflection tunnel; native path parsing is lossless and both native transports reject
  noncanonical ASCII spellings as `not_sent`;
- plan preconditions use the current-protocol snapshot schema over documented Song tempo/signature,
  exact Boolean Song transport-running, count-in, Arrangement Record, Session Overdub, Automation
  Arm, and both documented Arrangement-overdub properties, exact Boolean Link enablement, Link
  start/stop sync, Tempo Follower, tempo-nudge, Back to Arrangement, and Re-Enable Automation
  controls, plus exact Boolean Arrangement Loop and metronome state,
  the version-available current-scale tuple, the TuningSystem name/pseudo-octave and exact opaque
  note-range/reference-pitch/note-tunings dictionaries,
  exact Scene names and tempo/meter override state, tracks/returns/master, group membership, Clip
  occupancy/name/type/length/signature/marker/loop/activator and version-coupled launch state,
  exact Boolean Clip-envelope state and exact Boolean Clip playing/recording/overdub/triggered/
  will-record-on-start state,
  exact Boolean normal-Track freeze, arm, implicit-arm, and per-Track Back-to-Arrangement state,
  plus bounded fired/playing Session-slot indices,
  exact Boolean normal-Track audio/MIDI-input classification and conditionally exact selected and
  available input-routing type/channel dictionaries plus exact floating input/output hold peaks,
  exact selected normal/Return output-routing type and channel dictionaries,
  exact Boolean Return-Track mute, solo, and derived solo-mute state,
  nullable normal-Track Group parent identity,
  device display/runtime identity,
  role, activity, exact Rack-chain capability, and reported sample/millisecond latency, mixer
  volume/pan/send/Track-Activator
  internal/display values, finite internal bounds, quantisation, and exact
  enablement/activity/automation state, exact normal/Return
  crossfade assignments with a null Master assignment, and exact panning mode on every mixer,
  and cue points; tuning-dictionary semantics and Score mapping, per-track bypass, and instrument/MPE
  behavior, Delay Compensation/monitoring mode, Track Delay, complete routing, buffers/drivers,
  and external/acoustic latency remain residuals; the Scene
  array and every track clip-slot count must equal the scene count, exact object field sets are
  enforced; Clip runtime predicates are included, while playhead position and UI selection are
  excluded;
- after an executing apply, each Part's final Track name, audio/MIDI-input and audio/MIDI-output
  classification,
  ordinary/Push arm state, neutral Main-crossfader assignment, Stereo Pan mode, own mute/solo
  state, derived solo-mute state, exact in-range quantised enabled/active/unautomated Track
  Activator lowering, freeze
  state, and nullable Group parent are
  backpropagated from the post
  snapshot. The Mix
  gate intent is `!mute && (!any_project_channel_solo || solo)`; no Part gate can verify unless
  both arm states are false, per-Track Back-to-Arrangement is false, fired and playing slot indices
  are both -1, crossfade assignment is exactly 1, and panning mode is exactly 0. An
  expected-enabled channel cannot verify while `muted_via_solo` or `is_frozen` is true or a Group
  parent is present. Membership
  observation is explicit so null cannot mean both ungrouped and unobserved. The independent
  `monitoring_state_observed` and `clip_output_not_suppressed_by_monitoring_verified` residuals
  remain false because disarming does not observe monitoring and the current public Track LOM omits
  the selector that Ableton
  documents as capable of suppressing clips. Recording plans retain the same requests with null
  observations and do not claim verification;
- each generated Part requires its final public `input_meter_level` and `output_meter_level` to be
  exact floating zero. Both are documented one-second hold peaks, are conditional on an audio/MIDI
  Track, and participate in guarded-plan equality. Evidence separates observation, input/output
  hold quiescence, and their conjunction. The Track gate requires the conjunction, while
  `continuous_input_silence_verified` and `continuous_output_silence_verified` remain false because
  sequential bounded-window meters do not prove persistent event isolation, signal-path closure,
  interface/acoustic behavior, or rendered sound;
- each Part input-route record retains the final selected Live type/channel symbols and complete
  advertised option arrays. The Part gate requires exact `(has_audio_input, has_midi_input) =
  (false, true)` and full-pair membership of both selections. A normal Track with neither input
  class instead requires four explicit null route fields; Return/Main records omit them. Exact
  input selection and option ordering participate in stale-plan equality, but
  `input_source_identity_mapped` and `external_input_neutrality_verified` remain false. The source
  IR declares no portable Live input source, the public identifiers are opaque, and Sunny does not
  infer semantics from display text or perform an input-routing mutation;
- each Part and generated Return output-route record retains the requested Sunny destination as
  `Master` or `Group(id)` alongside the final selected Live type/channel display names and
  identifiers and the complete advertised type/channel option arrays. Every option is an exact
  `(display_name, identifier)` pair; `selected_type_available_verified` and
  `selected_channel_available_verified` require full-pair membership, and
  `selected_output_available_verified` is their conjunction. `selected_output_observed` and all
  three membership facts become true after a coherent final snapshot, but
  `source_target_identity_mapped` and route `verified` remain false because the public LOM
  publishes no portable semantic Main/Group identifier. Exact selection and option-set state
  participate in stale-plan equality; narrower Track/Return gain gates do not misrepresent Live
  option membership as route realization;
- every generated AuxBus retains its exact appended Return Track index and requested name,
  false mute/solo state, neutral crossfade assignment 1, Stereo Pan mode 0, and scalar return pan.
  Aggregate final evidence requires the same Return name and gate/mode values plus
  `muted_via_solo = false` and an in-range quantised, enabled, active, unautomated Track Activator
  1.0; the return level and pan are independently rechecked as unquantised, enabled, active,
  unautomated mixer parameters. This closes
  the tractable generated wet-path gain stage at the
  sequential observation point, not target-owned output routing, non-pan spatial fields, signal,
  or rendered sound;
- the source MasterBus has no mute or pan intent, but Live's Main MixerDevice has an independent
  Track Activator, panning mode, and panning parameter. Mix compilation therefore writes and reads
  the neutral lowering `(activator, mode, pan) = (1.0, 0, 0.0)`. Aggregate final evidence exposes a
  separate Main gate requiring an in-range quantised, enabled, active, unautomated activator and an
  in-range unquantised, enabled, active, unautomated pan plus Stereo Pan mode and centered pan. This
  closes only the tractable final gain/spatial stage; target-owned
  routing, cue/crossfader behavior, signal, persistence, and sound remain residuals;
- every planned native insertion also retains a final-snapshot device obligation: exact
  mixer-excluding chain cardinality, requested index/display identity/public type, active state,
  false `can_have_chains`, and the exact bounded Device latency report. The Rack discriminator
  keeps the flat serial source model from verifying against target-owned parallel/nested chains.
  `reported_latency_observed` records the latency pair while
  `render_path_latency_fully_observed` remains false; no sum or compensation state is inferred.
  Missing, extra, moved, renamed, retyped, or inactive devices remain successful target divergence
  but make the aggregate incomplete; malformed stored intent is a protocol error;
- every generated slot-0 Session Clip retains the compiler's requested name, MIDI role, finite
  unlooped marker interval/derived length, initial meter, and active state. Snapshot comparison
  verifies those structural properties after the complete deployment. The independent loop brace
  remains unobserved and dormant while `looping = false`; admitting looping would require exact
  loop-start/end evidence. On Live 11+, each generated
  Clip additionally receives null `groove` association with immediate readback and final
  `has_groove = false` evidence. It also receives Trigger launch mode, no clip quantization,
  Legato off, and zero launch-velocity scaling with immediate/final evidence. Those scalar facts
  produce `launch_behavior_verified`; the public Clip LOM does not expose Follow Actions, so
  `follow_actions_observed`, `one_shot_playback_verified`, rendered timing, and rendered velocity
  remain explicit false residuals. It likewise exposes no MIDI Clip bank/sub-bank/program state,
  so `midi_bank_program_state_observed` and `program_change_suppression_verified` remain false:
  final Device/parameter equality does not prove that launch-time MIDI cannot select another
  preset. The current snapshot schema also retains exact Boolean `is_playing`, `is_recording`,
  `is_overdubbing`, `is_triggered`, and `will_record_on_start` state for every occupied Clip.
  Generated-Clip evidence requires all five false and exports independent
  `recording_quiescence_verified` and `playback_idle_verified` verdicts. These are sequential
  point observations, not a stop mutation, atomic tuple, future-stability proof, or playback trace.
  The same Clip record requires exact audio false/MIDI true/Arrangement false identity, on
  Live 11+ exact Session true identity, and on API-qualified Live 12+ exact Take-Lane false
  identity. Live 11 already has take lanes; this adapter retains null below Live 12 because its
  take-lane API is unqualified there. Null does not verify false identity or content absence.
  For the required unlooped state, public
  `end_time` must independently equal the requested End Marker through
  `playback_end_verified`; neither derived equality is a playback trace.
  The containing Track independently retains the complete ordered ClipSlot vector, including empty
  cells. Exact cardinality/index order and exact equality between `has_clip` occupancy and the
  separate Clip records are structural obligations. Generated-Track evidence requires every slot
  to be non-Group, status-zero/not playing, not recording/not will-record, and not triggered; it
  exports the complete vector plus independent non-Group, playback, recording, launch, and combined
  runtime-quiescence verdicts. Live's documented status equivalences are validated on both wire
  peers. Stop Button presence is retained target state but is not requested Sunny state. No slot
  fire/stop action, atomic Session tuple, future guarantee, recording preference, playback trace,
  signal, or sound is inferred.
  The same evidence compares the complete observed occupied-slot index set with the exact request
  `{0}`; an additional valid Session Clip on the generated Track makes that Clip postcondition
  incomplete without deleting target-owned content.
  Session occupancy is not Arrangement occupancy. On Live 11+, the containing Track also retains
  the exact cardinality of its separate `arrangement_clips` list and the generated-Track gate
  requires zero. Earlier modeled profiles retain null and cannot verify the absence proposition.
  This count is complete for the requested empty set but does not identify arbitrary nonempty
  Arrangement content, make the sequential reads atomic, or establish playback or sound.
  On Live 12+, the Track also retains the separate `take_lanes` cardinality and requires zero for a
  generated Track; below Live 12 the count is null and take-lane absence remains unverified.
  Take Lane Audition Mode can replace the normally audible main lane, but no public
  LOM audition-state property exists; eliminating the lane topology is therefore the complete
  tractable proposition, not evidence that an unobserved switch is off.
  Each generated Clip also retains the requested all-envelope clear, its immediate
  optional Boolean observation, and final `has_envelopes = false` obligation. This closes the
  public Clip-envelope absence proposition without authoring Score automation or proving MPE
  expression, external control, or audible output. Per-note MPE is independently unobservable in
  the current public Clip note dictionaries, so `mpe_note_expression_state_observed` and
  `mpe_note_expression_neutrality_verified` remain false even when the ordinary note subset and
  Clip-envelope absence verify. Because the current snapshot schema
  omits note collections, each inserted batch is then re-queried with the closed nine-field
  population-readback shape. Live 11.1+ uses `get_all_notes_extended`; Live 11.0 uses
  `get_notes_extended` with all pitches `[0,128)` and note starts in `[0, requested_end_marker)`
  quarter-note beats. Its `observed_time_span` records that finite interval and
  `entire_clip_population_observed` is false: notes outside it remain unobserved. Exact insertion
  IDs and the retained property multiset can verify within that interval, but the complete batch
  verdict also requires whole-Clip population access. Snapshots omit notes on every version, so
  snapshot equality itself cannot establish note-content drift or absence outside a query;
  unsupported batches remain unverified;
- final Song tempo/meter and Scene-0 name/disabled tempo-meter overrides are compared with the
  compiler's stored property intent. It also retains the complete ordered Scene `is_triggered`
  vector and requires every value false. This is a finite point-in-time pending-launch predicate,
  not a Scene stop/fire mutation, atomic Session-state tuple, future-stability, playback, or sound
  proof. Final Song evidence additionally requires false count-in,
  Arrangement Record, Session Overdub, Automation Arm, `arrangement_overdub`, and `overdub` state;
  `transport_stopped_verified` is reported independently and is not a structural-completeness
  requirement. No transport/record setter is issued, and the under-specified integer
  `session_record_status` is not interpreted. It also requires Link, Link start/stop sync, Tempo
  Follower, and both nudge controls false through
  `public_tempo_controls_quiescent_verified`, without issuing a setter. Incoming MIDI-sync
  enablement and tempo automation state remain unobserved, so `tempo_stability_verified` remains
  false. Exact `back_to_arranger` and `re_enable_automation_enabled` observations must also be false,
  respectively establishing `arrangement_playback_aligned_verified` and
  `automation_overrides_quiescent_verified` at the sequential observation point without invoking a
  repair. Exact Song `loop` and `metronome` must also be false, establishing
  `arrangement_loop_disabled_verified` and `metronome_disabled_verified` without a toggle. Loop
  start/length remain dormant and unobserved under that false invariant. Every projected top-level
  section must resolve to exactly one
  final CuePoint within the bridge's `1e-7` beat identity window with the requested name;
- final-value intent for every Score/Mix mixer parameter path is collapsed in actual phase order
  and compared with the current snapshot schema. Equality verifies only when the observed quantised/
  continuous class matches the operation, `is_enabled` is true, and DeviceParameter `state` and
  `automation_state` are both zero. Internal-value requests must also lie in the observed finite
  bounds; display-value requests retain but do not compare against those internal bounds. The
  number of normal-Track sends must equal the observed Return
  Track count. Send-level requests/configurations remain separate from the independent pre/post-mode
  requests/configurations, so a scalar level write never counts as a complete configured send;
- every mapped Timbre/Mix native DeviceParameter retains a self-contained logical obligation and,
  after its containing Device verifies structurally, receives a current-protocol exact-name read-only
  observation. Final verification requires resolved identity, selected value, enabled state,
  active state, and no automation; internal-value mappings also require final `min`/`max` equality,
  while display-value mappings retain those internal bounds with a null range-equivalence verdict;
- selected Score properties, mapped Timbre/Mix parameters, and Mix scalar properties require immediate set/readback evidence from
  a real current-protocol bridge; mapped Mix insertion also requires observed pre-insert device counts,
  while mapped parameters additionally retain active and automation states and recording transports
  retain the plan with null observations;
- channel/group relative faders are solved as exact additive-dB constraints before Score creates
  the first target object; LUFS-relative nodes remain explicit measured-audio residuals;
- Mix Channel/Group/Aux output edges are internally closed under the X10 bidirectional-membership
  invariant. Without explicit routing bindings they remain requested residuals. Both project
  planning and immediate compilation accept exact Part/Aux-to-Master type/channel dictionaries
  with mapping provenance. The projection validates current target membership, writes type then
  channel, and requires exact readback. Unbound and Group edges remain residuals; Track
  audio-output classification is not destination evidence;
- public LOM has no project transaction, group-track creation operation, arbitrary browser-load
  operation, or automation-envelope authoring operation used by Sunny;
- Max for Live availability is not inferred from the Live version and remains `unknown` unless a
  future target supplies independent evidence.

No Max object, Max external, `.amxd`, or RNBO export participates in the current aggregate target.
Their absence is a specified boundary, not an implicit fallback.

## 7. Atomicity, repeatability, and concurrency assumptions

Project validation and planning are atomic with respect to Sunny's synchronous in-process view.
The target observation is assembled sequentially during one synchronous main-thread bridge call;
the public LOM provides no snapshot transaction, so even internal instant-consistency is not
claimed. Live deployment is not atomic: public LOM exposes incremental mutations and no rollback transaction. A transport
failure after the first acknowledged or indeterminate operation may leave a partially modified Set;
the journal reports that fact but cannot undo it. One stored plan cannot be applied twice. Creating
and applying a new plan is not globally idempotent because Score compilation creates tracks/clips.

Snapshot equality is an optimistic precondition, not a Live lock. Another actor can mutate Live
after the fresh pre-apply read. Exact command guarding prevents Sunny from sending a command that
its own runtime derivation changed, while the post snapshot records selected resulting structure
and verifies tractable Song/Scene/Cue state, Track-level output/gate state of generated Parts,
structural state of their generated Session Clips, and exact final chains containing every planned
native insertion.
An external same-shape edit outside the selected documented fields remains possible and is not
misrepresented as tractable.

The MCP server serialises `process_request` calls per server instance and holds that serializer
through synchronous tool-handler execution. This applies both to the stdio loop and to concurrent
calls through the public parsed-request seam. Tool registration completes before request handling,
and shared Score/Timbre/Mix/Corpus session values therefore have one exclusive request owner. A
bound authoring request prepares codec-created private candidates, stages them in the canonical
identity-stable slots while retaining the original values in a rollback guard, and commits only
after cross-document validation and immutable history preparation. Errors, exceptions, and counter
overflow restore the originals with swaps and preallocated map nodes. Other requests cannot observe
the staged candidate. Direct access to mutable session stores is not a concurrent-reader API.
A future parallel dispatcher must use immutable aggregate publication rather than separately
publishing sibling documents before claiming to preserve the same preflight guarantee.

## 8. Tractability boundary

| Property | Static/runtime tractability | External evidence required |
|---|---|---|
| Local Score/Timbre/Mix validity | Fully tractable in C++ | None |
| Exact Part correspondence | Fully tractable in C++ | None |
| Order-independent downstream targeting | Fully tractable with command-buffer tests | None |
| Wire request syntax and capability skips | Contract-testable without Live | Official LOM documentation |
| Live acknowledgement of issued commands | Observable through the bridge | Running named Live build |
| Selected Score/Timbre/Mix scalar target state | Contract-testable; immediately observable through current-protocol readback on a live target, including finite bounds, quantisation, enablement, activity, and automation state | Named Live build and retained deployment evidence |
| Generated Part Track output/gate state | Post-snapshot name, audio/MIDI-output role, exact own mute/solo, ordinary and Push arm states false, Main-crossfader assignment 1, Stereo Pan mode 0, in-range quantised enabled/active/unautomated Track Activator lowering, unfrozen and observed ungrouped state, and absence of derived solo-mute when Mix expects the channel enabled | Named Live build and retained postcondition evidence; disarming does not observe Monitor In, and Group processing, routing, and current public-LOM monitoring state remain outside the selected gate |
| Generated Aux Return Track output/gate state | AuxBus-ID/index binding, post-snapshot name, exact own mute/solo false, absence of derived solo-mute, crossfade assignment 1, Stereo Pan mode 0, Track Activator 1.0, and correct-domain enabled/active/unautomated equality for activator, return level, and pan | Named Live build and retained postcondition evidence; non-pan spatial fields, output routing, later changes, signal, and sound remain outside the claim |
| Main Track neutral output stage | In-range quantised enabled/active/unautomated Track Activator 1.0; Stereo Pan mode 0; in-range unquantised enabled/active/unautomated pan 0.0 | Named Live build and retained postcondition evidence; output routing, cue/crossfader behavior, later changes, signal, and sound remain outside the claim |
| Generated Session Clip structural state | Post-snapshot exact audio/MIDI and Arrangement identity, Live-11+ Session identity, Live-12+ Take-Lane identity, name, unlooped marker interval/derived length and `end_time`, meter, loop flag, activator state, version-coupled absence of an associated groove, and Boolean absence of Clip envelopes | Named Live build and retained postcondition evidence; dormant loop brace and rendered timing/velocity remain outside the claim |
| Generated Session Clip note membership | Sequential post-deployment complete-note query; exact insertion-returned ID set and requested eight-field property multiset | Named Live build and retained postcondition evidence; MPE, tuning, playback, and sound remain separate |
| Global Song/Scene and projected Cue state | Post-snapshot tempo/meter, Scene-0 name and disabled launch overrides, quiescent public Link/Tempo Follower/nudge controls, no Session-over-Arrangement playback divergence, no advertised automation override, disabled Arrangement Loop/metronome, plus unique time/name evidence for every requested top-level CuePoint | Named Live build and retained postcondition evidence; incoming MIDI-sync enablement, tempo automation content/persistence, dormant loop bounds, later changes, and nested section hierarchy remain residuals |
| Final channel/return/master mixer scalar state | Current-snapshot-schema selected internal/display value, finite internal bounds, quantisation, enabled, active, and unautomated state for every final volume, pan, activator, and enabled-send intent, with exact panning mode retained | Named Live build and retained postcondition evidence; generated Part, Aux Return, and Main Tracks require final Stereo Pan mode, while routing/destination and audible signal remain separate |
| Planned native-device final chain state | Post-snapshot exact chain size and per-index display identity, public type, activity, false Rack-chain capability, and bounded sample/millisecond latency report for every insertion deployment | Named Live build and retained postcondition evidence; Rack support requires an explicit recursive source/target contract, and the report does not prove compensation, total path latency, routing, or sound |
| Complete rendered timing | Not derivable by summing Device reports; public Song/Track LOM omits compensation/monitoring mode and Track Delay, while routing, buffers/drivers, external hardware, and acoustic propagation remain open | Explicit named-build playback/render experiment with a defined reference event and complete path conditions |
| Mapped native-device parameter durability | Sequential current-protocol exact-name query after structural Device verification; identity/value/enabled/active/unautomated equality, plus internal range equality where applicable | Named Live build and retained postcondition evidence; observation is non-atomic and does not prove modulation, signal, or sound |
| Global pitch context | Current-snapshot-schema current-scale tuple plus every documented TuningSystem property, with the four dictionaries retained exactly but opaquely, version-gated and stale-plan compared | Dictionary semantics and Score mapping, mutation, Track bypass, device/MPE support, and audible pitch remain unverified |
| Mix output destination edges | X10 validates intent; explicit Part/Aux-to-Master bindings admit ordered type/channel set/readback with mapping provenance; unbound and Group edges remain residuals | Named-Live dictionary membership and exact set/readback evidence; signal and sound remain separate |
| Resulting audible timbre/mix equivalence | Not established by scalar readback | Rendered audio plus listening/measurement criteria |
| Stale project/selected target structure | Guarded by canonical state and exact snapshots | Named Live build for host behaviour |
| Exact planned command order | Fully tractable and guarded before every send | Live acknowledgement for execution |
| Partial-failure provenance | Journalled; failed mutation effect remains explicitly indeterminate | Post-failure target access improves evidence |
| Atomic rollback/global idempotence | Not supplied by current LOM target; one plan object/identifier is one-shot | New target-side transaction/reconciliation design and Live tests |
| Max for Live presence or device behaviour | Unknown in the current target | Independent licence/device discovery and named Live/Max validation |

The strongest repository-only result is internal validity plus contract-tested translation against
an offline model of Live's documented API. Live end-to-end conformance is established only by the
final live validation against a running Live.
