# Sunny reference

This is the concise implementation reference. Musical definitions and IR invariants are normative
in `docs/formal`; software layering is defined in `docs/architecture.md`.

## 1. Names and modules

The public API follows semantic paths rather than an external component registry:

| Layer | Namespace | Public headers | Implementations |
|---|---|---|---|
| Theory and IRs | `sunny::core` | `include/sunny/core` | `src/core` |
| Deterministic rendering | `sunny::render` | `include/sunny/render` | `src/render` |
| Bounded Max control transfer | `sunny::max` | `include/sunny/max` | `src/max` |
| Adapters and application services | `sunny::infrastructure` | `include/sunny/infrastructure` | `src/infrastructure` |

Types and enum classes use `PascalCase`. Namespaces, functions, variables, fields, and filenames
use `snake_case`. Established public constants use `UPPER_SNAKE_CASE`.

## 2. Result and error model

Fallible native operations return `Result<T>`, an alias for
`std::expected<T, ErrorCode>`. `VoidResult` is the corresponding `void` result. Exceptions do
not cross native module or protocol boundaries.

The single taxonomy is declared in `sunny/core/types/music_types.hpp`:

| Range | Domain |
|---|---|
| 2xxx | Input and value validation |
| 3xxx | Music-theory computation |
| 36xx | Render controls, pattern state, PPQ, and scheduling |
| 41xx–43xx | Connection, transport, and MCP |
| 45xx | External formats |
| 5xxx | Score IR |
| 6xxx | Timbre IR |
| 7xxx | Mix IR |
| 8xxx | Corpus IR |
| 9xxx | Cross-IR project model |

Unchecked value operators are for already-validated internal data and assert when their result
cannot be represented. Trust boundaries use checked factories or checked arithmetic and return an
error.

## 3. Fundamental values

### 3.1 Pitch

- `PitchClass` is a validated element of Z/12Z. `from_int` rejects invalid runtime input;
  `wrapped` is reserved for explicitly modular operations.
- `MidiNote` accepts 0–127 and never wraps.
- `SpelledPitch` preserves letter, accidental, and octave, so enharmonic identity remains
  distinct from sounding pitch.
- `DiatonicInterval` stores chromatic and diatonic displacement together.

### 3.2 Time

`Beat` is an exact rational with a signed 64-bit numerator and positive 64-bit denominator.
Every value is irreducible by construction; components are observed through `numerator()` and
`denominator()` and cannot be mutated independently. A quarter note is `Beat{1, 4}`.

- Comparison uses a continued-fraction algorithm and never forms overflowing cross-products.
- Multiplication and division cross-cancel before multiplication.
- `checked_add`, `checked_sub`, `checked_mul`, and `checked_div` report
  `ArithmeticOverflow`.
- `Beat::from_ratio` is the checked boundary for dynamic integer pairs. It rejects a zero
  denominator and an unrepresentable canonical result; the two-argument constructor is for
  already-admitted program values and enforces the same canonical representation as a contract.
- `Beat::from_float` accepts only finite input and a positive denominator bound, uses a checked
  bounded continued-fraction approximation, and returns `InvalidBeat` or `ArithmeticOverflow` on
  failure. The Python constructor canonicalises and exposes read-only properties.
- Shared Score/Timbre/Corpus Beat JSON emits lowest terms. Readers may canonicalise an equivalent
  fraction, after which another write is byte-structure stable at the JSON value level.
- `absolute_beat_to_tick` rounds to the nearest tick and saturates at the signed 64-bit limits
  without overflowing an intermediate product.

`PositiveRational` is the distinct exact-rate value used by tempo events. It admits only positive
components, reduces them at construction, exposes `numerator()` and `denominator()` as read-only
observations, and provides `PositiveRational::from_ratio` for dynamic input. Consequently
`PositiveRational{240, 2}` is the same stored value as `{120, 1}`, and Score tempo JSON is canonical
before conversion to an effective quarter-note rate or a floating host tempo.

`TimeSignature` owns a non-empty positive grouping and a positive power-of-two denominator. Its
safe default is 4/4; `groups()` and `denominator()` are read-only, and
`TimeSignature::from_groups` is the checked dynamic boundary. `has_distinct_meter_grouping`
reports whether flattening to numerator/denominator would erase a partition not recoverable from
Sunny's deterministic flat-signature constructor. This is representational evidence, not a claim
about a host's audible accents.

The score time chain is:

```text
ScoreTime (bar, offset) → Absolute Beat → Real Time
                                  └────→ MIDI Tick
```

`DEFAULT_PPQ` is 480. Score bars are one-indexed.

### 3.3 Identifiers

`Id<Tag>` provides strongly typed document identifiers. IDs from unrelated document domains
cannot be compared accidentally. Default IDs are non-owning sentinel values; document workflows
allocate real identities. Score component authoring indexes the candidate document and chooses the
lowest unused positive Event, Part, or Section ID. This makes allocation deterministic, admits
deserialised high IDs without collision or wrap, and means a rejected candidate consumes no global
allocator state. Reductions that replace event content restart their independent event namespace
at one; part extraction indexes every retained source event before allocating cues. `ScoreId{0}` is
the unassigned root sentinel. Creation accepts a repository-selected positive root ID, and every
derived-score API requires its output ID explicitly. MCP uses one checked `uint64` sequence for
both the session handle and stored root identity, admits the maximum value once, and never wraps.

## 4. Document contracts

Sunny owns four typed intermediate representations:

| IR | Current schema | Main entry points |
|---|---:|---|
| Score | 8 | create, mutate, query, validate, compile |
| Timbre | 2 | create, route sources/effects/modulation, map target parameters, validate |
| Mix | 3 | create graph, route, automate, validate |
| Corpus | 4 | ingest, analyse, profile, query, validate |

Deserialisation is a trust boundary. Readers validate field presence, enum ranges, rational
denominators, identifiers, and structural/document invariants before returning a document.
Serialisers emit the current schema version.

`ProjectView` is the non-owning aggregate over one Score, a set of TimbreProfile references, and
one MixGraph. It is not a fifth persisted IR. Its formal contract is
`docs/formal/sunny-project-model.md`.

Corpus schema 4 is field-complete for the current C++ document. It persists separate finite,
non-negative onset and duration quantisation RMS evidence; v1–v3 migration supplies zero for the
historically absent duration field without treating that default as a measurement. Composer and period style profiles
are synchronously rebuilt from unique analysed memberships; C15 rejects a stale deterministic
non-signature aggregate. Rebuild formulas consume explicit WorkAnalysis evidence across all nine
domains, including metre, tonal-plan, resolution, density/dynamic, orchestration, and
transformation evidence. `note_count` distinguishes an empty melodic sentinel from a measured
range, and absent dynamic evidence retains a neutral stationary arc. Fields with no work-level
carrier remain zero/false/empty as an unavailable marker, not an observed stylistic absence. The
formal Corpus specification defines every denominator, rank, tie, and evidence-presence rule.

This internal freshness claim is separate from Ableton/Max conformance. No bridge operation imports
a Live Set into Corpus, writes a StyleProfile to Live, or treats Max output as WorkAnalysis. A
profile may inform an agent's ProjectView authoring; only the resulting Score/Timbre/Mix intent is
eligible for the guarded Live deployment and readback contract.

### 4.1 Score mutation and undo

Score mutations validate before commit and return `MutationResult`. The native score undo stack
stores document-safe inverse operations and preserves identity.

`set_section_harmony` constructs a real `ChordVoicing`, not only an analysis label: all members of
the registered quality must fit MIDI, notes are strictly ascending, and a supplied slash bass must
be a chord member whose exact register becomes the voicing bass and inversion. Each Roman numeral
uses the key and mode active at that entry's own position. Unknown qualities, non-chord slash
basses, incoherent inversions, and partial out-of-range voicings fail before state, version, or undo
history changes. S15 validates the persisted chord payload and S21 validates its copied key context.

Live orchestration has a separate history model. Each operation stores typed forward and inverse
bridge-message batches. Undo and redo enqueue those batches; they do not retain closures or capture
an `Orchestrator` pointer.

### 4.2 Compilation

`compile_to_midi`, `compile_to_musicxml`, and `compile_to_lilypond` validate their input and
return a compilation report with diagnostics. Invalid or unrepresentable temporal data is reported
instead of silently relocating or dropping it.

`compile_to_midi` emits all four SMF Time Signature values. Its in-memory denominator is the actual
power-of-two value; the file writer encodes the exponent. The current exact Score profile accepts
numerators 1…255 and denominators through 128, writes `bb=8`, and chooses
`cc=96*gcd(groups)/denominator` when that is an integral byte. This is a metronome-click
projection, not an ordered-group encoding. Time-signature event requested counts include global
map points and nonredundant equal-duration Part-local residuals. Grouping requested counts include
only non-default source partitions, while SMF grouping written remains zero; `has_drops()` observes
either shortfall. Corpus MIDI ingress requires complete same-tick payload agreement, rejects
`bb!=8`, and records a `cc` mismatch as manual-correction evidence instead of converting it into
Score grouping. Ableton uses the same explicit-grouping definition at its separate flat
numerator/denominator boundary.

Compilation reports expose both `has_drops` and `has_residuals`. The former is the narrow summary
of dropped-event counters and requested/written shortfalls. The latter additionally includes every
diagnostic-only degradation, including targets that emitted a usable approximation while losing a
semantic distinction. MCP `complete` uses `has_residuals`; a client does not need to duplicate the
summary rule by inspecting the diagnostic array itself.
Standalone Score and aggregate Project responses use the same private native encoder for this
object. The report field set and its two summary Booleans therefore have one implementation
authority rather than parallel handler-local JSON definitions.

The same private evidence authority serialises Core validation diagnostics across Score, Timbre,
Mix, Corpus, and aggregate Project tools. Every diagnostic contains `rule`, `severity`, `message`,
and numeric `error_code`; `location` and `part_id` are present exactly when populated. Standalone
Score and aggregate Project Ableton results likewise share the exact Score-tuning, requested and
observed note, Clip-envelope, CuePoint, scalar-property, Timbre-parameter, device-insertion, Mix
Return/Main Track, Mix-parameter, and Mix-parameter-coverage deployment schemas. Public contract
tests pin their complete object shapes and compare standalone Timbre/Mix evidence with aggregate
Project evidence compiled from the same stores. This is provenance and schema alignment: it does
not turn an observation reported by Sunny into independent Live/Max host evidence.

Corpus MIDI ingress also treats channel state as a typed boundary. When all source notes use one
wire channel, the generated Part preserves it as the corresponding 1-based rendering channel.
Completed CC64 and CC67 switch pairs on that channel become `SustainingPedal` and `UnaCorda`
PartDirective spans. Values in the MIDI-defined off/on ranges are accepted semantically, while
noncanonical bytes are reported under `midi.pedal_switch_values` because recompilation emits
0/127. `midi.control_change_events` covers unsupported, cross-channel, repeated, same-tick
ambiguous, and unmatched CC messages; `midi.program_change_events` covers every raw Program
Change because Score has no arbitrary tick-positioned program carrier. Multiple note channels and
Type-1 tracks are separately reported as ownership/topology loss. Low-level `MidiFile` remains the
exact model; the Corpus Score is intentionally an analytical projection.

`resolve_grace_timing` is the shared performance projection for grace notes. Appoggiaturas occupy
their full positive Score allocation; acciaccaturas are capped at `Beat{1,32}` and right-aligned
inside that allocation. MIDI, `NoteEvent`, SMF, and Ableton note deployment therefore agree on the
same non-negative start and duration. MusicXML and LilyPond preserve symbolic grace notation and
do not claim identical target playback.

Generic `NoteEvent` has no tie flag. `compile_to_note_events` therefore uses the same validated
tie-chain resolver as MIDI/Live: one event begins at the head, spans the checked exact sum of every
segment, takes attack state from the head, and takes release velocity from the terminal segment.
Consumed continuations still update the Voice's running semantic dynamic for later attacks. A
stable onset sort preserves deterministic Part/Voice/note order among simultaneous events.

That projection also shares the note-local articulation stages. Unmapped articulations use exact
default ratios (`1/2` staccato, `1/4` staccatissimo, `3/4` portato, `17/20` marcato, and `7/4`
fermata) and the shared velocity offsets. Custom velocity layers and duration scales are applied;
the stored float scale is interpreted as its exact IEEE-754 rational and rejected if it cannot fit
`Beat`. Keyswitch, CC, and Program Change children have no `NoteEvent` field, so each affected note
gets a location-bearing compilation diagnostic instead of silent omission. MIDI/Live retain the
same note-local shaping while separately accounting for target-specific control deployment.

Score schema 8 adds a closed structured harmony path: `ChordSymbolEvent` may carry a one-based
numeral root, integer alteration, explicit five-mode numeral key, inversion, and ordered
add/alter/subtract degrees. S25 proves that the numeral derives the exact same spelled root as the chord.
`score_insert_chord_symbol` authors that point event through the native transactional mutation.
MusicXML emits its `numeral`, `numeral-key`, `inversion`, and `degree` fields directly; legacy
Roman/extension strings remain presentation-only residuals and are never parsed into semantics.
The compact `MusicXmlScore` reader/writer and Corpus ingester now consume the same typed subset at
an exact measure-local offset. Both call the core numeral-to-root derivation used by S25. They
reject deprecated `function`, stacked chords, frames, fractional alterations, non-primary harmony
staffs, and unretained harmony attributes rather than silently flattening them.

The full MusicXML and LilyPond targets also account for populated document-level state that their
admitted notation profiles do not carry. A hierarchical `SectionMap`, global harmonic analysis,
orchestration analysis (including rendering-only `dynamic_balance`), a governing tone row, and
stale harmonic/orchestration regions each produce an explicit diagnostic. Event-level
`ChordSymbol` export is not treated as preservation of the distinct persisted analysis layer.

Ableton score compilation produces Live Object Model requests. Clip note insertion uses the
documented dictionary payload:

```json
{
  "notes": [
    {
      "pitch": 60,
      "start_time": 0.0,
      "duration": 1.0,
      "velocity": 100,
      "mute": false,
      "probability": 1.0,
      "velocity_deviation": 0.0,
      "release_velocity": 64.0
    }
  ]
}
```

Section markers use the closed `sunny_set_cue` adapter. All source positions are preflighted before
target access. A live response identifies whether the adapter created or updated a cue and carries
requested/observed time and name; compilation exposes requested, created, updated, and verified
counts plus each deployment record. Offline command recording reports `recorded_only` and does not
claim a Live mutation or readback. Only top-level section starts become CuePoints; total, projected,
and unprojected node counts retain the nested hierarchy loss explicitly.

Score deployment also exposes paired requested/written cardinalities for compiled notes, tempo
events, time-signature events, and articulation controls. Requested note count is the shared MIDI
projection's Live-note payload count; written means a successful adapter call, not a
property-equality claim. Real note calls must additionally return exactly one unique integer note
ID per requested dictionary, then satisfy a closed `Clip.get_notes_by_id` query for ID plus
pitch/start/duration/velocity/mute/probability/velocity-deviation/release-velocity and a closed
`Clip.get_all_notes_extended` query for that same field set. Both observations must be identical
and contain exactly the created IDs, so extra or
missing Clip notes cannot masquerade as a verified batch. Per-Part deployment records retain the
exact requested eight-field tuples, returned IDs, observed notes, cardinality/property verification
flags, and distinguish `inserted` evidence from an offline `recorded_only` batch. Thus the
verification proposition can be reconstructed without recompiling the Score; recording plans also
retain exact note intent while claiming no observation, and a pre-Live-11 target uses the distinct
`unsupported` action without losing that intent. `note_batches_verified` and
`notes_verified` count exact multiset equality;
all execution/readback counters remain zero for recording transports. Later tempo and meter map points remain requested and
produce capability warnings because the admitted bridge writes only the Song's current scalars.

Protocol v43 fixes only the adapter-owned fields to floating probability 1.0 and velocity
deviation 0.0. Score schema 8 retains schema 7's integer `release_velocity` in `[0,127]` with neutral default 64;
the bridge projects it as an exact integral-valued JSON float and verifies it beside the other five
Score-derived properties. The low-level `MidiFile`, generic `NoteEvent`, Corpus ingester, MIDI
compiler/file writer, Live compiler, render schedulers, and `sunny.events` Max adapter all retain
that state. In a tie chain, attack velocity comes from the first segment and release velocity from
the terminal segment. Thus valid nonzero SMF Note Off intensity is no longer rejected or replaced.
The full MusicXML and LilyPond notation profiles do not claim a release-velocity playback carrier:
they emit a location-aware diagnostic for every non-default value. Their compact `NoteEvent`
adapters, which cannot return residual reports, reject such a value instead of erasing it.
They resolve semantic attack markings from either coherent Score carrier (`Note.dynamic` first,
then `VelocityValue.written`). If neither carrier exists and `VelocityValue.value` is a nonzero
explicit MIDI attack byte, both full notation compilers emit a location-aware residual. MusicXML's
documented sound `dynamics` value is a percentage of forte velocity 90 rather than an exact byte,
and the admitted LilyPond profile has no exact per-note byte construct, so neither compiler invents
an approximate playback value.
Part-local meter regrouping with the same exact bar duration remains notation-only metadata and is
reported as a dropped meter event; an unequal local bar duration is rejected by MIDI, NoteEvent,
and Ableton performance compilation before target access because those paths use one global
ScoreTime axis.

Key signatures have their own requested/written pair at the Live boundary; the current written
count is zero. Live 12 documents current-scale properties, but safe deployment still requires
active-tuning compatibility, a closed accepted scale-name mapping, interval readback, and stale-plan
snapshot state. Until those conditions are modelled, global/local key intent remains explicit and
Live scale-aware devices are not claimed aligned.

Score schema 8 retains schema 6's `ScoreTuning`: an exact reference note/frequency and one relative-cent position
for every note index 0–127. `score_tuned_frequency` is the normative finite projection, S28 closes
its domain, `score_set_tuning` authors it atomically, JSON persists it, and every derived Score
copies it. `parse_scala` still retains general Scala interchange data; `scala_to_cent_table` is the
older partial twelve-degree/octave projection, while `scala_to_score_tuning` expands any non-empty
positive-period Scala scale under an explicit reference-degree and adjacent-key recurrence.

MIDI, NoteEvent, MusicXML, and LilyPond treat only canonical 12-TET/A4=440 as their admitted nominal
baseline; a different complete pitch function yields requested/written tuning counts and a
diagnostic. Ableton results always retain the exact `requested_tuning`, report one requested and
zero written definitions, and cannot be complete on that dimension. Live 12.1's writable
`live_set tuning_system` surface still requires closed dictionary member schemas and verified
write/readback before Sunny may mutate it. Existing target tuning can reinterpret otherwise exact
note indices, so `notes_written` is dictionary acceptance—not frequency or audible equivalence.

Timbre and Mix results follow the same rule for structural intent. Source devices, effects, Group
and Return Tracks, sends, and channels expose a requested count beside the successful mutation
count. This makes an absent request distinguishable from a preserved plug-in/GroupBus/old-Live
capability gap; creation and insertion counters remain call outcomes unless separate readback
evidence is present.

The individual deployment tools are `score_compile_to_ableton`, `compile_timbre`, and
`compile_mix`. `project_validate`, `project_plan_to_ableton`,
`project_apply_ableton_plan`, and `project_compile_to_ableton` resolve those documents from shared
session stores, enforce exact Part correspondence before mutation, and target Timbre/Mix operations
through a Score-derived `PartId → Live track index` map. Planning assembles one structural target
observation during a synchronous main-thread call and performs an exact offline command dry-run;
applying consumes that plan once, rejects changed project/target state, guards every command, and
returns a mutation journal. The convenience
compiler performs those two phases immediately. A
successful response has `success: true`; `complete` is true only when every requested feature is
representable through the supported Live Object Model surface. Capability gaps are returned in
`warnings` or the retained Score MIDI `report`, and no guessed property or method is emitted. The
production source of truth is the validated `(Score, TimbreProfiles, MixGraph)` tuple: these own
musical/performance-event, device/timbre, and routing/mix intent respectively. Aggregate
`complete` includes Score report drops/diagnostics and target tuning residuals; it does not imply
playback, waveform, hardware-output, or acoustic equivalence. Score note deployment targets Live 11+.
The retained Score report carries authoritative `has_drops` and `has_residuals` Booleans;
`complete` consults the latter so diagnostic-only degradation and counted omission have one closed
composition rule.

`project_apply_ableton_plan` and `project_compile_to_ableton` optionally accept
`validation_context` with required printable strings `live_edition`, `operating_system`,
`architecture`, and `remote_script_revision`, a required `cleanup_steps` string array, and an
optional all-or-none `max` object containing `max_version`, `max_for_live_version`, and
`license_state`. This context is validated before plan consumption or target mutation. Responses
that reach the native one-shot apply return `validation_record_context_supplied` and
`validation_record`; the latter is null when context was omitted and otherwise contains strict
`AbletonValidationRecord` schema 1 for the actual attempt. Pre-plan validation, lookup, connection,
or planning failures have no attempt to wrap. Environment and cleanup values are explicitly
operator-supplied. A true
`deployment.execution_trace_complete` requires an acknowledged journal entry for every planned
request and is false for offline command recording; it is not the compilation `complete` verdict
and is not a sonic or authenticity claim.

Native-device insertion in the Timbre and Mix compilers targets Live 12.3+ and native Live devices.
For a Timbre source, `devices_created` is insertion-call evidence while `devices_verified` requires
an exact count/index response whose observed class display name matches the request, whose public
Device type is instrument, and whose `is_active` state is true. `device_deployments` retains the
requested name, append index, and requested public Device type as well as those observations plus
Track `has_audio_output`/`has_midi_output`; verification additionally requires
Live to report instrument-generated audio output rather than residual MIDI output. Offline plans
claim no verification. Source insertion first requires an observed
empty mixer-excluding device chain, because neither a newly created nor caller-supplied track is
assumed free of default/existing devices.
On a real peer, only a verified source may anchor Timbre effect and parameter addresses; a
well-formed but unverified source stops those dependent mutations while preserving their requested
counts/deployment residuals. Recording plans continue through modeled structure without claiming
verification.
Timbre effects use the same `device_deployments` sequence and expose
`effects_requested`/`effects_inserted`/`effects_verified`; each verified effect is an exact active
audio-effect append that preserves Live's audio-output Track classification.
Mix effects now use that same structural evidence rule across channel, aux-return, and master
chains. Their `effects_verified` and `device_deployments` are separate from Timbre's result and from
per-parameter verification.
Mix output routing is separately loss-accountable. Results expose
`output_routes_requested`/`output_routes_written`/`output_routes_verified` and exact
`output_route_residuals` for every Channel, Group, and Aux edge. With no binding, the compiler
reports zero writes/verifications because Live routing values are target-owned dictionaries and
the public creation functions promise no Main-routing default; `has_audio_output` is not treated
as a route to Main. `compile_mix`, `project_plan_to_ableton`, and the project convenience compiler
accept an optional closed `output_routing_bindings` object with `part_tracks` and `aux_returns`
arrays. Each item identifies the typed IR source, an exact type/channel
`{display_name, identifier}` pair, and non-empty `mapping_provenance`. Only materialisable
Channel/Aux-to-Master edges may bind; Group
destinations remain residuals. A bound edge emits two ordered, journalled mutations, with type and
channel membership revalidated against the target's current advertised collections and exact
readback required at each stage. Results retain every stage verdict and `not_applied`,
`recorded_only`, or `set`; only complete real evidence removes the residual. Group member lists and
child routing references must first pass the X10 exact-mirror invariant.
The core group workflows preserve that invariant themselves: duplicate requested members fail
before mutation, creating a group moves already-assigned channels by removing every old reverse
edge, and reassignment removes stale or duplicated listings from every group before adding the one
new edge. Nested-group assignment and return-to-Master routing apply the same cleanup to
`member_groups`, validate the complete candidate transactionally, and reject cycles or excessive
depth without partial mutation. X10 remains the independent validator for deserialised or directly
constructed graphs.
Aux-send identity is equally closed before projection. X11 requires every Channel/Group send level
to be finite and each source to contain at most one record for a given AuxBus; X6 requires the
target AuxBus to exist. Enabled GroupBus sends increment `sends_requested` but not
`sends_configured`, with an explicit warning, because their unmaterialised Group Track has no valid
Live mixer path. Every enabled Channel/Group send also contributes one level and one mode request.
The current bridge reports successful Channel scalar writes only through
`send_levels_configured`; it reports zero `send_modes_configured` and therefore zero complete
`sends_configured`. Both `pre_fader = true` and `pre_fader = false` are explicit mode obligations,
so neither may be inferred from a target default.
On a real target, an unverified Mix effect cannot anchor a parameter path: mapped source/target
values remain in an unobserved deployment, but the parameter mutation is not sent. Recording plans
retain the modeled command sequence.
Each compilation result also contains `target_profile`: the observed Live version, bridge protocol,
adapter runtime/contract, and explicit `available`, `unavailable`, or `unknown` capability states.
Successful compiler results make this field mandatory. A missing profile is a pre-mutation protocol
error; offline command recording uses an explicit modelled profile rather than granting itself the
newest capabilities from absent evidence. The same consistency validator runs at each compiler
boundary, including for custom transports that did not obtain the profile through JSON parsing.
The session-state tool exposes the same profile. Max for Live is `unknown` because the documented
Application version API does not report licence/edition state.
An apply attempt consumes its move-only native plan object and stored MCP plan identifier even when
a project/target precondition then fails; reuse returns status `plan_consumed` and error 9006 before
target access. If the MCP transport is unavailable, apply is not entered and the plan remains
unconsumed.
The Mix effect algebra includes dedicated `delay` and `reverb` variants, both available through
all four Mix effect-authoring tools. Mix schema version 3 requires every effect to carry a
`parameter_map` object and migrates schema versions 1 and 2 to empty maps without inventing target
facts; an unknown discriminator or invalid mapping is rejected at the JSON trust boundary.

Timbre schema version 2 makes target parameter conversion explicit. Current schema version 3 adds
the owned modulation-envelope collection and migrates versions 1 and 2 to an empty collection
without inventing a source. `map_timbre_parameter`
declares an IR path and source interval, a curve, a target interval, the exact Live parameter name
and device index, and whether the target is DeviceParameter `value` or `display_value`. Compilation
resolves the current IR value before mutation, emits the exact converted value, and returns
`parameter_deployments` with the IR source, requested target/range/property/name, explicit
`not_applied`/`recorded_only`/`set` action, resolved current/original name, observed value/internal
bounds, quantisation, enabled/active/automation state, and a scalar verification flag. Score compilation
returns `property_deployments` for initial Song tempo/signature, Scene-0 title and disabled
tempo/meter overrides, every created Clip's finite active unlooped marker/loop/activator state,
Live-11+ Trigger/None/Legato-off/velocity-zero launch tuple and null groove association,
independently installed signature, track/clip names, and optional pan. Mix
compilation returns the same evidence shape for fader, pan, mute, solo, send, return, and master
property writes. `map_mix_effect_parameter` declares the source interval, curve, target interval,
exact target name, and value/display domain on the effect that owns the source. Mix compilation
queries the existing device count, derives the exact inserted device path, and returns
`parameter_coverage` plus deployment/readback evidence. Recording-only transports retain null
observations; real protocol-v41 transports must supply well-typed evidence.

Timbre scalar paths have one exact structural resolver. Its canonical grammar is an identifier
followed by zero or more decimal indices, repeated with dot-separated identifiers (for example
`source.oscillators[0].tune_cents`). It addresses only continuous `float` leaves present in the
active source/effect variant. It rejects ignored suffixes, absent optionals, out-of-range or
non-canonical indices, enums, counts, identities, and topology. Get/set, modulation, automation,
preset, rendering-map, and validation paths therefore cannot disagree about whether an address
exists. Automation additionally requires a defined interpolation and one or more finite,
strictly-time-ordered breakpoints at or after score start; the same predicate guards mutation and
deserialization.
Clip envelope deletion is not a generic property deployment. Score results expose
`clip_envelope_clears_requested`, `clip_envelope_clears_executed`, and
`clip_envelope_clears_verified`, plus `clip_envelope_deployments` retaining Part/track identity,
requested absence, optional observed `has_envelopes`, `recorded_only`/`cleared` action, and verdict.
The exact no-argument protocol-v41 call invokes the public all-envelope clear and accepts only an
exact one-Boolean response; a void acknowledgement is insufficient state evidence.
Timbre and Mix compilation report `automation_lanes_requested` and
`automation_lanes_written` separately. The current public-LOM target preserves every requested lane
in its source IR, reports zero written lanes, and returns an incomplete result; the API never uses a
single zero count that could be mistaken for either “nothing requested” or “request satisfied.”

`set_channel_relative_level` authors exact channel/group additive-dB constraints transactionally;
`resolve_mix_fader_levels` returns the dependency solution for all channel, group, and master
faders. Static relations are applied during Mix compilation. A `master_target` LUFS relation is
reported as `requires_loudness_measurement`, its dependants are reported as blocked, and their
stored absolute values remain explicit fallbacks. `set_channel_level` selects absolute semantics
and clears a previous channel relation.

## 5. Render contracts

The maintained render surface is formally specified in
`docs/formal/sunny-render-model.md`.

- `Lfo`, `Envelope`, and `SampleAndHold` validate finite control/sample domains. Random LFO output
  has a fixed default seed and an explicit replayable seed contract. `Lfo::set_waveform` rejects
  unknown integer-cast enum values without replacing the prior waveform.
- `SignalBlockContext::create(sample_rate, maximum_frames)` validates the setup facts required by a
  host signal-vector adapter. The `Lfo`, `Envelope`, and `SampleAndHold` block forms admit only spans
  no larger than that maximum, validate before mutation, and perform no allocation/locking/I/O.
  LFO/ADSR vectors equal their corresponding scalar call sequence exactly; the held source repeats
  one retained message value across the admitted vector. Python `process_block` is an allocating
  convenience and is not the native audio-thread surface. Maximum and actual frame counts are
  signed until `validate_frame_count` rejects negative/unrepresentable/oversize values and returns
  a checked `size_t` extent.
- `Lfo::validate_context` separately checks retained frequency against a candidate setup rate. A
  DSP rebuild or control transfer can therefore reject incompatibility before admitting candidate
  processing state. `Lfo::set_frequency(frequency, context)` validates a control candidate
  against the active rate and commits it atomically without resetting recurrence state; block
  processing repeats the guard without partial output. The adapter still owns bounded transfer.
- The host-shaped modulation overload accepts a signed count and raw output pointer specifically so
  it can validate count first, reject null for a non-empty vector, and only then construct the
  internal span. Empty/null is a successful no-op; C++ still relies on the adapter for the actual
  host array/channel shape and allocation extent.
- The complete Max-shaped generator overload accepts signed frame, input, and output counts plus
  both pointer arrays. It requires exact `numins == 0` and `numouts == 1` before any pointer access;
  the zero-input array is never dereferenced, and only a non-empty output requires `outs` and
  `outs[0]`. This models a traditional mono modulation-generator topology, not MC, the complete Max ABI, or
  buffer provenance. A host adapter preserving source time must keep the perform routine registered
  when its outlet is disconnected; otherwise the call-driven recurrence freezes. The public
  `signal_input_count`/`signal_output_count` constants expose the exact fixed topology. The
  message-triggered held source uses the same topology and emits its retained scalar across the
  complete admitted vector; it is not a signal-triggered sampler.
- `sunny::max::{LfoAdapter,EnvelopeAdapter,SampleAndHoldAdapter,ClockAdapter,ItmEventAdapter}`
  provide the concrete bounded ownership seams through the `sunny::max_adapter` CMake target.
  All message/control callers are serialized by one producer-side mutex before entering a fixed
  64-command SPSC queue; the audio owner takes no mutex and applies at most 64 ordered commands at
  the next vector boundary. Queue saturation is `RenderControlQueueFull`, processing before a
  successful `dsp64`-shaped setup is `RenderNotConfigured`, and status counters retain separate
  accepted/applied/rejected/setup-failure/process-failure evidence plus the current pending-control
  gauge. Callback topology and storage are validated before pending controls are drained. A
  rejected rebuilt context invalidates processing/configured state while preserving recurrence
  state and every queued control; LFO rebuild admission checks every pending frequency. A later
  admitted rebuild can therefore continue the retained source state without ever processing the
  new host chain under stale setup metadata.
  Clock controls are tempo, tick position, play, record, pause, and stop; they affect only the local
  BlockClock at the next valid vector edge.
  The distinct ITM adapter serializes publishers into 64 commands and reserves at most 256 events;
  one Max scheduler consumer applies them against a finite global-transport tick/resolution
  snapshot. Its status distinguishes producer-pending commands, reservations spanning pending and
  retained events, and actual scheduler-retained cardinality. It is not an audio adapter, and none
  of these source gauges is host callback evidence.
- `max-package/` is an independent, opt-in Cycling '74 SDK project pinned to one max-sdk-base
  revision. It recompiles the authoritative adapter/render translation units under that toolchain
  rather than mixing MSVC runtime modes. It builds traditional `sunny.lfo~`, `sunny.adsr~`,
  `sunny.hold~`, and `sunny.clock~` wrappers plus the non-DSP `sunny.events` object on
  macOS/Windows. Every wrapper rejects a null class, accumulates every `class_addmethod` result,
  frees an unregistered partial class on error, and assigns its global class pointer only after
  successful `class_register`; `sunny.events` registers its hidden time-slot dependency before its
  public class. The four signal objects always register their perform routines, pass the callback's
  signed counts and arrays into the adapter, and use one bounded current-maximum silence path when
  that exact rebuild was rejected or processing fails. The package is staged under the build tree.
  SDK-header compilation is source
  evidence; only a named Max host can establish ABI loading, buffer provenance, disconnected
  processing, deadline behavior, or Max for Live participation.
- `MaxValidationRecord` schema 1 is the strict post-run evidence envelope for one such named host.
  It binds current Sunny/package versions, source/package hashes, exact host/audio settings, the
  ordered five binary artefacts, and 19 ordered checks with content-addressed evidence. Its
  `complete` value is derived from all artifact and applicable-check outcomes; it is not an
  operator assertion. Standalone and Max for Live versions have complementary transport-identity
  applicability. `sunny_native.validate_max_validation_record_json` is a thin entry to the native
  parser. A valid or complete record is not self-authenticating and does not generalise to another
  platform or scheduler configuration. Reserved staged-example provenance remains parser-valid but
  can never derive `complete`.
- `MaxValidationObservation` schema 1 is the corresponding unhashed host-fact envelope. It has no
  package/archive digest, binary digests, evidence digests, or `complete` claim.
  `sunny-max-evidence assemble OBSERVATION PACKAGE_ARCHIVE PACKAGE_ROOT EVIDENCE_ROOT` validates
  that envelope, rejects missing/non-regular/escaping file leaves, derives every SHA-256, and emits
  canonical `MaxValidationRecord` JSON. The `verify` command accepts the same three file inputs and
  rejects any digest mismatch. It verifies bytes and paths, not the truth or authorship of a host
  outcome.
- `tests/max_sdk_headers/validate_staged_package.py` is the shared macOS/Windows CI artifact gate.
  It requires the exact five external entries and nonempty binaries, exact help/reference and
  max-test patcher/validation sets, the pinned harness manifest, package manifest/readme, and
  byte-identical staged authored content. Synthetic tests cover both layouts and
  deletion/drift/addition failures. It does not replace the native evidence parser or a host run.
- The four MSP wrappers preserve their signal as outlet 0 and emit `host_status configured
  sample_rate maximum_frames process_observed process_calls last_frame_count process_healthy
  last_error` from
  message outlet 1. `sunny.events` preserves event lists as outlet 0 and emits `event_status
  command_observed scheduled_observed fired_observed cancelled_observed callbacks_healthy
  reserved_events retained_events last_error` from outlet 1. These are machine-readable,
  non-transactional source observations; they do not establish deadline, connection, scheduler,
  MIDI, or sound behavior by themselves.
- `misc/validation/max-test-harness.json` pins Cycling '74 `max-test` revision
  `8c5d833d4b1e454238ced7c866c34acedb69cc07`, package version 1.2.1, its three-table SQLite result
  contract, the exact staged smoke/helper patchers, and every expected assertion. The callback-driven
  standalone smoke maps 14 checks to 64 produced assertions, leaves four `not_automated`, and marks
  `live_transport_discontinuities` `not_applicable`; it cannot derive record completeness.
- `misc/validation/export-max-test-sqlite.py MANIFEST DATABASE TEST_ID` is a thin read-only raw-row
  projector. It requires a regular quiescent database with no journal/WAL/SHM sidecar, an exact
  positive run identity, required columns, a completed test, admitted outcomes, and stable bytes
  across extraction. `sunny-max-evidence apply-max-test MANIFEST RESULT OBSERVATION EVIDENCE_ROOT
  DATABASE_RELATIVE_PATH` independently hashes the manifest and retained database, validates the
  closed C++ `MaxTestRunResult`, requires all 64 exact assertion names, derives only the 14 mapped
  pass/fail facts, and emits canonical `MaxValidationObservation` JSON. Python never decides a Sunny
  check outcome.
- `misc/validation/max-release-matrix.json` is the exact schema-1 release topology: standalone Max
  and Max for Live on macOS x86_64, macOS arm64, and Windows x86_64. Its coverage scope is each
  record's exact environment. `sunny-max-evidence assemble-matrix MANIFEST RECORD_ROOT` reads the
  six fixed `records/*.json` paths, embeds strict complete records, binds their exact bytes, requires
  one source/package revision, and requires both host kinds on each native target to use the same
  archive and binary hashes. `verify-matrix MATRIX MANIFEST RECORD_ROOT` re-hashes the manifest and
  record files. It does not replace the six per-record `verify` operations against archives,
  extracted packages, and evidence roots.
- `misc/validation/max-host-run-plan.json` is the closed external-run procedure manifest. It pins
  the `max-test` repository and SDK-submodule revisions, a 48 kHz/512/64 fully enabled baseline,
  1,000 active-DSP teardown cycles per object, five scheduler scenarios with a 32-event exact
  impulse-spacing criterion, two exact release-velocity MIDI note pairs, four floating-PCM
  comparisons, and six Max for Live transport-discontinuity scenarios. It is a measurement plan,
  not an observation.
- `misc/validation/index-max-host-run.py` hashes the plan and the twelve closed shared residual
  artifacts into `host-run-result.json` while copying exact observation provenance; for an M4L
  environment it additionally requires the named-host-saved `.amxd`, exact pinned assertion map,
  64-assertion projection, ordered six-scenario measurements, and host console. It derives no
  outcomes. `prepare-m4l-device-source.py` creates inventoried editable Max Audio Effect source
  from the pinned smoke and upstream console helper, removing the standalone `dac~` DSP-start edge
  and declaring a silent unconnected `plugout~`; `export-m4l-assertions.py` projects one exact marked
  console session while hashing its manifest, device, and transcript. Neither file is host evidence
  until a named build saves and runs the `.amxd`. `sunny-max-evidence apply-host-run` requires the
  compiled plan/assertion-map digests, re-hashes all transitive inputs, decodes the timing and render
  IEEE-float WAVE files, reconstructs impulse offsets, maps the 64 facts to five artifacts and
  thirteen shared M4L checks, and derives the four residual facts plus conditional discontinuity
  fact. The discontinuity result must cite the assertion run ID. `verify-host-run` replays the same
  validation. A measured miss becomes `failed`; malformed, provenance-divergent, or drifting
  evidence is rejected.
- `misc/validation/prepare-max-host-run.py` validates the package ZIP sidecar and exact
  archive/extracted-tree bytes before creating one non-overwriting cell workspace. It fills exact
  environment/provenance and complementary host applicability but leaves every applicable check
  `not_run`; `operator-inputs.json` retains array-form standalone extraction, M4L source/export,
  indexing, native application, transitive verification, assembly, and record-verification
  commands.
  `run-named-live-validation.py` likewise defaults to a read-only named-Live plan, and
  with explicit `--apply` plus cleanup steps retains the complete JSON-RPC transcript and non-null
  `AbletonValidationRecord`. Every run requires the installed Remote Script root and Live log;
  it content-addresses the server and every tree file and retains stable before/after log snapshots.
  Neither helper changes the record algebra or authenticates host facts.
- `sunny_max_archive` is the Max-package distribution target. It depends on all five external
  targets, archives the stage under one `Sunny/` root, and emits
  `Sunny-<version>-<platform>-<architecture>.zip` plus `.zip.sha256`. The staged-package validator
  requires exact ZIP member/set/byte equality and independently checks that sidecar.
- A new `SignalBlockContext` does not reset any processor. LFO phase/random state, Envelope stage,
  and BlockClock position/fraction carry into the next admitted block at the new rate. The current
  control adapters retain that state across admitted `dsp64` rebuilds; another stop/start reset
  policy would require an explicit adapter operation and lifecycle evidence.
- `Arpeggiator` rejects empty or out-of-MIDI-range patterns, invalidates its locked cache when
  pitch-affecting configuration changes, and uses a specified seeded permutation.
- `generate_arpeggio` converts the floating gate once, then uses checked rational onset and duration
  arithmetic.
- `BlockClock` validates PPQ, tempo, position, and signal vectors, retains fractional ticks across
  blocks, and returns committed integer endpoints in O(1) without allocation, locks, I/O, events,
  or callbacks. Its signal form emits local quarter-note position at the start of each sample and
  commits the identical endpoint. Paused/stopped vectors hold the retained fractional position.
  `sunny.clock~` packages exactly that local recurrence; it does not observe Max transport or Live
  song position. `Transport` composes BlockClock with integral-tick note scheduling. Equal-tick
  note-offs precede note-ons. `Beat` remains a whole-note fraction, while
  `TransportPosition.to_quarter_notes()` exposes the MIDI/Live coordinate; at 480 PPQ, 480 ticks is
  `Beat{1,4}` and 1.0 quarter-note beat.
- `BlockEventScheduler` is the fixed-capacity event counterpart: 256 inline tick events, caller-owned
  output storage, no callbacks or dynamic allocation, and deterministic sample offsets for the
  half-open local interval of each running vector. It quantises an event to the start of its
  containing sample, defers an exact post-vector event, and rejects an undersized event span before
  clock, queue, or output mutation. This is native planning evidence, not a Max event outlet or
  Ableton timing claim.
- `ItmEventAdapter` and `sunny.events` close an alternative Max scheduler path. `event tick pitch
  velocity [release_velocity]` and atomic `note tick pitch duration_ticks velocity
  [release_velocity]` commands target permanent
  locations on the unnamed global ITM transport. The wrapper acquires and references that ITM once
  before successful construction, retains it through command-clock and permanent-slot use, then
  dereferences it during teardown. The hidden time-slot class uses Max's documented
  `TIME_FLAGS_TRANSPORT` attribute default, and command publication serializes the corresponding
  command-clock wake under the same producer mutex. Sunny ticks map as
  `double(tick * host_resolution / ppq)` only while adjacent source ticks remain distinguishable;
  targets must be strictly future when the scheduler applies the command. Equal-tick callbacks are
  grouped in the same deterministic order and emit one-shot
  `[pitch attack_velocity_or_zero release_velocity]` lists. In Max for
  Live the unnamed transport follows Live, but actual sample accuracy remains conditional on
  Overdrive, Scheduler in Audio Interrupt, timely high-priority arrival, downstream object support,
  and named-host evidence. A pending `clear` retains its reservations until applied, so later
  publication can conservatively report capacity saturation rather than speculate about the
  consumer's future state. Scheduling an event requires a non-null host scheduling action; clearing
  retained work requires a host cancel action; and firing an active group requires a host stop
  action. Absence rejects transactionally, so the scheduled/cancelled/fired counters cannot be
  fabricated by a callback-free adapter call. Callback invocation still does not prove that Max
  accepted, delivered, or audibly realised the event.
  `event_after delay_ticks ...` and `note_after delay_ticks ...` compute the first Sunny tick whose
  mapped host value is strictly greater than a fresh ITM sample, then add the nonnegative delay.
  They retain the later application-time strict-future check, so scheduler latency fails closed
  instead of producing a knowingly late event. `transport_status` exposes current tick,
  resolution, running state, and SDK name from the retained global ITM as a
  sequential observation, not a host-identity or sample-accuracy proof.

Transport callbacks are tick-boundary and block-quantised. Because they carry no intra-block sample
offset, this surface does not claim sample-accurate host delivery, PCM rendering, Live transport
control, or Max/MSP scheduler integration. The host-shaped overload accepts a
`SignalBlockContext` and rejects `sample_count > maximum_frames` before position/fraction mutation
or callback dispatch. An admitted vector still dispatches at its endpoint; setup cardinality is
not event-placement evidence.

The bounded modulation surface also has Max-shaped forms taking signed frame/channel counts and raw
pointer arrays. The complete form validates frames, exact zero-input/one-output topology, then the
non-empty output array/channel pointer before selecting channel zero or constructing a span. The
input array is never dereferenced. This closes source-side count and dereference ordering; it does
not prove that a pointer originated from Max or names an allocation of the claimed extent.

All stateful render instances are exclusively owned and internally unsynchronised. Direct
modulation setters/triggers/queries must not race block processing. The Max adapters preserve this
rule by publishing validated commands rather than invoking the processor from message threads.
Transport callbacks observe the committed BlockClock endpoint, must not throw, and must not mutate
the same Transport reentrantly.
`Transport::process_block` is not real-time safe: it may drain an unbounded queue and invoke
computationally unconstrained callbacks, while scheduling can allocate. A Max perform adapter may
use the bounded modulation and BlockClock recurrences only after it supplies safe ownership/control
transfer; it must not call the current Transport surface from `perform64`. A validated vector bound
is not by itself a thread-safety or bounded-work proof.

BlockEventScheduler removes the source-side absence of offsets but not its host boundary: no
producer-to-audio transfer consumes its caller-owned result. `sunny.events` does not consume those
offsets; it delegates placement independently to permanent Max ITM locations. Neither path may be
called “sample accurate” without evidence appropriate to its actual downstream mechanism.

`BlockClock` integrates its locally owned constant tempo; it does not observe Live transport phase.
An eventual Max for Live integration must select and verify the Live clock source, distinguish
unnamed from named transports and per-device instances, handle preview/seek discontinuities, and
avoid deriving exact song position from a single BPM or a Max beat-time translation across Live
tempo changes.

## 6. Protocols

### 6.1 MCP over stdio

`sunny-mcp` implements JSON-RPC 2.0 over standard input/output, one JSON object per line. It
supports both protocol eras: `server/discover` and per-request metadata for MCP 2026-07-28, plus
the `initialize` handshake for 2024-11-05 through 2025-11-25 clients. The tool-serving methods are
`tools/list` and `tools/call`. Standard output is protocol-only; diagnostics go to standard error.

The six registration groups expose 116 user-callable tools:

| Registration group | Count | Scope |
|---|---:|---|
| Core and Ableton | 10 | theory, clip creation, session perception, Live undo/redo |
| Score | 29 | document/tuning/harmony authoring, articulation mapping, queries, validation, export, Live deployment |
| Timbre | 23 | sources, effects, modulation, target parameter mapping, Live deployment |
| Mix | 28 | routing, relational faders, processing, target mapping, intent, reference comparison, Live deployment |
| Corpus | 22 | ingestion, lifecycle/removal, analysis, style profiles, examples |
| Project | 4 | cross-IR validation, guarded plan/apply, ordered Live deployment |
| **Total** | **116** | |

Schemas are defined beside each registration call in `src/infrastructure/mcp/*_tools.cpp`; this is
the authoritative tool-name and parameter inventory. `McpServer::register_tool` preserves native
JSON Schemas and normalises the older compact `{name: "type (optional)"}` registration notation,
so every advertised `inputSchema` is an object schema with `properties` and `required` arrays.
The server validates those advertised types and required fields before invoking a handler, returns
the native JSON value in `structuredContent` as well as backward-compatible text, and marks input,
business, transport, and compilation failures with `isError: true`.

`score_validate` returns both `structurally_compilable` and `midi_compilable`. Its legacy
`compilable` field aliases the structural predicate. MIDI compilation additionally rejects
rendering-domain errors while retaining explicitly degradable event loss in its compilation
report.

### 6.2 Ableton TCP bridge

The native endpoint is `sunny::infrastructure::TcpTransport`; the peer is
`remote_script/Sunny`. The default port is 9001.

Each frame is:

```text
4-byte unsigned big-endian payload length
UTF-8 JSON payload
```

Both peers reject a payload larger than 16 MiB before buffering or writing its body. The native
transport uses bounded connect/read/write timeouts, handles interrupted system calls, and
reconnects on demand after a late Live startup or a Remote Script restart.

The Python socket thread performs framing and parsing only. Live Object Model access follows an
explicit queued/started/completed state machine on Live's main thread: a scheduling deadline may
cancel a request only before it starts, while an already-started call is awaited to a definite
outcome. Protocol v43 request and response envelopes carry the version on every exchange. For
example:

```json
{"bridge_protocol_version": 43, "type": "get", "path": "song", "name": "tempo"}
```

has the successful response:

```json
{"bridge_protocol_version": 43, "success": true, "value": 120.0}
```

and failures have exactly
`{"bridge_protocol_version": 43, "success": false, "error": "description"}`.

Native responses also retain a local delivery proof: `not_sent`,
`sent_without_valid_response`, or `response_received`. The deployment journal uses `not_sent` for
a definite `declined_before_send`; a failure after bytes may have left the process remains
`indeterminate` even when no valid peer response arrives.

The synchronous request algebra is exactly `get`, `set`, and `call`, over the closed operation and
canonical-path table in `docs/ableton-max-conformance.md` §2.1. It is not an arbitrary LOM
property/method tunnel. `get` accepts no arguments and
cannot invoke a method; `set` requires exactly one argument and returns its immediate observed
readback; `call` addresses methods. Missing
properties and malformed requests fail rather than returning a successful no-op. Observation was
removed because the bridge has no asynchronous event channel. Before deployment, `TcpTransport::target_profile`
calls the bridge's `sunny_get_target_profile`. Bridge protocol version 43 returns:

```json
{
  "bridge_protocol_version": 43,
  "adapter": {
    "name": "Sunny Remote Script",
    "runtime": "control_surface_python",
    "contract": "version_coupled_private"
  },
  "live": {
    "version": {"major": 12, "minor": 3, "bugfix": 5, "string": "12.3.5"}
  },
  "capabilities": {
    "clip_add_new_notes": "available",
    "track_insert_device_native": "available",
    "automation_envelope_authoring": "unavailable",
    "group_track_creation": "unavailable",
    "arbitrary_browser_loading": "unavailable",
    "structural_snapshot": "available",
    "max_for_live": "unknown"
  }
}
```

A successful generic `set` response has the exact nested evidence shape
`{"property": P, "requested": X, "observed": Y}`; missing or additional members are invalid.
The echoed and observed values must retain the requested LOM scalar category: integral, floating,
Boolean, and string are distinct. Integral equality is exact; only finite floating readback uses
the documented numeric tolerance.
Collection cardinality is never inferred from serialized Live objects. The no-argument
`sunny_get_scene_count` and `sunny_get_return_track_count` calls return non-negative integers;
raw `scenes`, `tracks`, and `return_tracks` gets are outside the algebra. The Python response
serializer recursively accepts only exact JSON-safe values, rejects non-finite numbers,
non-string object keys, integers outside the wire domain, and unsupported private objects, and
never converts such objects to strings. The profile, structural snapshot, cue, Device, and
DeviceParameter adapters likewise require target strings, integers, and floats in their exact
documented Python categories instead of using scalar constructors to conceal a host mismatch.
Protocol v43 retains the closed crossfade MixerDevice setter: normal and Return Track paths ending in
`mixer_device` accept only exact integer `crossfade_assign = 1`. This is Live's documented
“neither A nor B” enum value; Boolean, floating, A (`0`), B (`2`), Master, or other MixerDevice
properties remain outside that operation. The same paths plus the Master MixerDevice admit only
exact integer `panning_mode = 0`, Live's Stereo Pan mode. Split Stereo (`1`), Boolean, and floating
forms are rejected because Sunny owns one pan scalar rather than independent left/right positions.
Normal Track paths additionally admit only exact Boolean `arm = false` and
`implicit_arm = false`; true, integral, Return, and Master forms are rejected. These two setters
close Live's public ordinary and Push arm states for generated playback Tracks without claiming
that the separate Monitor selector is observed.
Return Track paths additionally admit only exact Boolean `mute = false` and `solo = false`. Their
Stereo Pan parameter path admits a finite scalar `value`, just as a
normal Track does. Protocol v43 does not admit Return arm or any derived-mute setter:
`muted_via_solo` remains read-only evidence of effective suppression by another solo.
Normal Track Activator paths admit only exact floating `value` 0.0 or 1.0; generated Return and
Main Activator paths admit only 1.0. The normal value is the target lowering of Boolean Channel
mute intent. The Main panning path separately admits only exact floating zero. Boolean, integral,
non-finite, arbitrary Main pan, and category-confused forms are rejected before object traversal.
Protocol v43's `sunny_set_device_parameter` adapter requires exactly these thirteen
members: `matched_name`, `original_name`, `property`, `requested`, `observed`, `minimum`, `maximum`,
`is_quantized`, `default_value`, `value_items`, `is_enabled`, `state`, and `automation_state`.
Exactly one domain member is populated: a non-quantized parameter carries a finite floating
`default_value` inside its reported internal range and null `value_items`; a quantized parameter
carries null `default_value` and an exact string array in `value_items`. The strings are retained
as opaque, possibly localized observations. Sunny does not infer their stability, uniqueness,
numeric correspondence, or semantic enum identity. Internal-value mappings
validate the declared range against Live's actual `DeviceParameter.min` and `max` before the
parameter write. State 2 is declined before writing; state 1 and non-zero automation state are
retained as incomplete deployment evidence. A real transport returning malformed or missing
evidence, including an object with an unknown member, is a protocol error; a recording-only
transport exposes null observations without
claiming verification.

Protocol v43 retains the strictly read-only `sunny_get_device_parameter(name, property)` operation.
Its exact twelve-member result is the setter result without `requested`: resolved current/original
identity, selected `value` or `display_value`, observed scalar, internal minimum/maximum,
the same conditional domain, quantisation, enabled state, active state, and automation state. It resolves one exact public or
original name, rejects zero/ambiguous matches and malformed host facts, but deliberately returns a
disabled, inactive, non-changeable, automated, or overridden parameter as observable divergence
without writing or re-enabling it.

Every successful `insert_device` response is also closed. In addition to the exact count/index,
identity, type, activity, exact Boolean `can_have_chains`, and Track output classification,
protocol v43 requires
`latency_in_samples` as a non-negative signed-LOM integer and `latency_in_ms` as a non-negative
finite floating value. `can_have_chains = true` identifies a Device Rack and makes the flat-device
verdict false; it is valid divergence, not a protocol category error. Results expose latency as
reported Device facts, not as a summed or compensated output-path latency.

Protocol v43 retains the v4 read-only `sunny_get_target_snapshot` call. It returns the profile plus
current tempo/signature, exact Boolean transport-running/count-in/Arrangement Record/Session
Overdub/Automation Arm and both documented Arrangement-overdub properties, exact Boolean Link
enablement, Link start/stop sync, Tempo Follower, tempo-nudge, Back to Arrangement, and
Re-Enable Automation controls, exact Arrangement Loop and metronome state, the version-available
Song scale tuple, and the documented TuningSystem name/pseudo-octave plus exact opaque
`lowest_note`, `highest_note`, `reference_pitch`, and `note_tunings` dictionaries, Session scene
count and Scene name/pending-launch/tempo/meter-override
state, ordered
tracks and returns, group membership, the version-coupled exact Arrangement Clip and Take Lane counts,
Session Clip occupancy and
name/audio-versus-MIDI identity/Arrangement identity/length/signature/marker/derived `end_time`/
loop/activator and version-coupled Session/Take-Lane identity plus launch state,
Boolean Clip-envelope state and the exact Boolean `is_playing`/`is_recording`/`is_overdubbing`/
`is_triggered`/`will_record_on_start` runtime tuple,
normal-track audio/MIDI-input and audio/MIDI-output classification, exact Boolean
freeze/arm/implicit-arm/back-to-arranger state,
bounded fired/playing Session-slot indices, nullable Group Track
parent, exact selected output-routing type/channel dictionaries, exact one-key available-output
type/channel collection wrappers, conditional selected input-routing type/channel dictionaries,
conditional exact one-key available-input type/channel collection wrappers, conditional exact
floating input/output hold-peak meter levels, and four conditional exact floating left/right
momentary input/output meters on audio-output Tracks, the complete ordered ClipSlot records with exact
occupancy/Stop Button/Group/control/runtime/status/will-record state, and
mute/solo/derived-mute state,
the same selected and available output-routing evidence on Return Tracks, and Boolean Return Track
mute/solo/derived-mute state,
device names/display classes/runtime classes, types/activity, exact Boolean Rack-chain capability,
and reported sample/millisecond latency, master
devices, mixer volume/panning/send/Track-Activator internal and display values with finite ordered
internal bounds, exact Boolean quantisation and enablement, the same conditional
`default_value`/`value_items` domain, plus parameter state/automation state,
integer `crossfade_assign` in 0–2 for
normal/Return mixers and `null` for Master, integer
`panning_mode` in 0–1 for every mixer, cue points,
and version-coupled Clip launch/groove association in snapshot schema 34. A normal Track
must expose exactly one send parameter per observed Return Track. The
field selection is grounded in the public [Song](https://docs.cycling74.com/apiref/lom/song/),
[Scene](https://docs.cycling74.com/apiref/lom/scene/),
[Track](https://docs.cycling74.com/apiref/lom/track/),
[ClipSlot](https://docs.cycling74.com/apiref/lom/clipslot/),
[Clip](https://docs.cycling74.com/apiref/lom/clip/),
[Device](https://docs.cycling74.com/apiref/lom/device/), and
[MixerDevice](https://docs.cycling74.com/apiref/lom/mixerdevice/) /
[DeviceParameter](https://docs.cycling74.com/apiref/lom/deviceparameter/),
[CuePoint](https://docs.cycling74.com/apiref/lom/cuepoint/), and
[TuningSystem](https://docs.cycling74.com/apiref/lom/tuningsystem/) contracts. Snapshot schema 34
requires the first three tuning payloads to be JSON objects and `note_tunings` to be the documented
single-member object containing a finite numeric array; it otherwise preserves their interiors
without inventing member names or semantics. This does not map them to `ScoreTuning`, authorize a
write, observe per-track Bypass Tuning, prove instrument/MPE behavior, or establish audible pitch. The Clip
runtime predicates are included, while playhead positions and UI selection state remain absent.
Generated Part evidence requires per-Track Back-to-Arrangement false, fired/playing indices -1,
exactly zero Arrangement Clip and Take Lane counts on Live 11+, and the generated Clip's complete
occupied-slot set exactly `{0}`. It also requires audio false/MIDI true/Arrangement false on every
modeled target, Live-11+ Session true/Take-Lane false, and an unlooped public `end_time` equal to
the requested End Marker. Ableton documents that Take Lanes are normally silent but one can
replace the main lane when Audition Mode is enabled; the public LOM exposes no audition-state
property, so zero lane topology is the tractable proof. Live 10 reports both counts as unobservable,
so the generated Track cannot pass that gate there. These are non-destructive
sequential observations, not a stop/delete action or a future-state guarantee.
It independently retains every ClipSlot record and requires the vector to be observed completely,
all slots non-Group, all playing statuses zero, all playing/recording/will-record predicates false,
and all trigger predicates false. Snapshot validation makes `is_playing` equivalent to nonzero
status, `is_recording` equivalent to status 2, requires non-Group status zero and no
`controls_other_clips`, and requires the slot `has_clip` set to equal the separate Clip records.
`has_stop_button` is observation-only. The resulting `clip_slot_runtime_quiescence_verified`
verdict detects a later empty-slot action but proves neither atomicity, future stability, a Live
recording preference, playback, signal, nor sound.
Each output-routing dictionary has exactly string `display_name` and `identifier` fields. Each
available collection has exactly its documented property name mapped to an array of those exact
dictionaries. The selected full type/channel dictionary must occur in its corresponding array;
otherwise the sequential adapter read is rejected as incoherent. Exact selected, available, order,
or symbol changes therefore invalidate a guarded plan. Final Track/Return evidence exports the two
available arrays plus `selected_type_available_verified`,
`selected_channel_available_verified`, and their conjunction
`selected_output_available_verified`. It pairs those facts with Sunny's requested `Master` or
`Group(id)` edge. Without a retained binding, `source_target_identity_mapped` and route `verified`
remain false: collection membership proves a Live-valid selection, not its semantic identity. A
bound materialisable Master edge retains the requested pair and provenance and verifies only when
the final selected dictionaries exactly equal that pair and remain advertised. The public contract
does not publish a portable destination identifier, and `has_audio_output` is only signal-type
classification; provenance is an explicit assumption, not a derivation from display text or a
newly created Track's default.
Input routing uses the same exact two-string option dictionaries and same-named one-key available
wrappers, but only on normal Tracks. Exact Boolean `has_audio_input` and `has_midi_input` determine
the closed union: if either is true, both selected input dictionaries and both available wrappers
must be present and each selected full dictionary must occur in its corresponding array; if both
are false, those four fields must all be null. Return and Main records have no input fields. A
generated Part requires observed `(has_audio_input, has_midi_input) = (false, true)` and exports
`selected_input_observed`, independent type/channel membership verdicts, their conjunction, and
the exact option arrays. It deliberately keeps `input_source_identity_mapped` and
`external_input_neutrality_verified` false. The public contract supplies opaque target symbols,
not a portable semantic input-source map, and Sunny neither interprets a localized “No Input”
label nor writes input routing. Membership and classification are sequential configuration
evidence, not monitoring, event-arrival, signal, or sonic evidence.
For the same audio/MIDI normal-Track branch, `input_meter_level` and `output_meter_level` must be
finite JSON floating values in `[0,1]`; both must be null when neither input-class flag is true.
Live documents each scalar as a one-second hold peak. Generated Part evidence requests exact
`0.0`, exports `meter_levels_observed`, independent input/output hold-quiescence verdicts, and their
conjunction. When `has_audio_output` is true, the four `input_meter_left`, `input_meter_right`,
`output_meter_left`, and `output_meter_right` fields must likewise be finite JSON floats in
`[0,1]`; otherwise all four must be null. Live describes these as smoothed momentary peaks and
warns that they load the GUI. Sunny therefore reads each once. Generated Part evidence requests
four more exact zeros and exports input/output stereo, combined momentary, and overall hold-plus-
momentary verdicts; only the overall verdict gates the Track. Integer zero is rejected rather than
silently crossing the LOM scalar category. `continuous_input_silence_verified` and
`continuous_output_silence_verified` remain false even when all six values are zero because the
reads are sequential and bounded in time and do not close meter sensitivity, event timing,
monitoring, routing, hardware interfaces, future activity, or acoustic/rendered output.
This is a sequential optimistic observation, not an
atomic host snapshot or lock. An apply requires exact equality with its planned observation before
mutation. Each issued command is then compared with the dry-run phase, type, path, name, and
arguments. Protocol-valid read-only observations are forwarded outside that mutation comparison
and journal, because some arguments depend on real acknowledgements. Journal outcomes distinguish
recording-only simulation, acknowledged target calls,
locally declined divergent calls, and failed calls whose target effect is indeterminate.
The public integer `session_record_status` is excluded because the current LOM reference does not
define its value mapping. Final Song evidence requires count-in and all selected record/overdub/
automation-record modes false, while exposing transport stop as an independent non-gating verdict;
the bridge issues no transport or record-mode setter.
It also requests all five public tempo controls false and gates Song verification on
`public_tempo_controls_quiescent_verified`, without issuing a setter. Incoming MIDI-sync
enablement and the tempo automation envelope remain outside the complete public Song record, so
`external_midi_sync_state_observed`, `tempo_automation_state_observed`, and
`tempo_stability_verified` are false. Exact tempo readback is current scalar equality, not an
exclusive-authority or persistence proof.
The exact Song record additionally includes Boolean `back_to_arranger` and
`re_enable_automation_enabled`. Final evidence requests false for both, exports
`arrangement_playback_aligned_verified` and `automation_overrides_quiescent_verified`, and gates
Song verification on both verdicts. The bridge calls neither Back to Arrangement nor Re-Enable
Automation; a true value is retained as well-formed divergence rather than repaired.
Boolean Song `loop` and `metronome` are also exact schema-17 fields. Final evidence requests both
false, exports `arrangement_loop_disabled_verified` and `metronome_disabled_verified`, and gates
Song verification on both. Sunny does not toggle them. Because Arrangement loop start and length
are inactive when `loop = false`, they are not added to the snapshot or promoted into active
playback claims.
The pitch-context fields are version-coupled: Live 12.0.5+ requires an exact non-null
`scale` object with `root_note`, `name`, nonempty bounded integer `intervals`, and Boolean `mode`;
Live 12.1+ additionally requires an exact non-null `tuning_system` object with `name` and finite
positive `pseudo_octave_in_cents`. Earlier versions require null for the corresponding object.
Every occupied Clip on Live 11+ requires integer `launch_mode` in 0–3, integer
`launch_quantization` in 0–14, Boolean `legato`, floating `velocity_amount` in 0–1, and Boolean
`has_groove`; older modeled targets require null for all five and are not probed. Generated Live
11+ clips request Trigger/None/Legato-off/velocity-zero plus `groove = null`, and final project
evidence requires exact equality and `has_groove = false`. It exports `launch_behavior_verified`
and groove absence separately from explicit false `follow_actions_observed`,
`one_shot_playback_verified`, `audible_timing_verified`, and `audible_velocity_verified`
conclusions. The public Clip LOM exposes no MIDI bank/sub-bank/program launch fields; generated
Clip results therefore also export `midi_bank_program_state_observed = false` and
`program_change_suppression_verified = false`.
Its public note dictionaries also expose no per-note MPE Pitch/Slide/Pressure curves or expression
clear operation. Generated Clip results therefore export
`mpe_note_expression_state_observed = false` and
`mpe_note_expression_neutrality_verified = false`; ordinary note-subset equality and Clip-envelope
absence do not promote either field.
Independently of Live version, every occupied Clip requires exact Boolean `has_envelopes`.
Generated clips request the public all-envelope clear before note insertion and require immediate
and final false observations. This is Clip-envelope absence evidence only; it does not subsume MPE
note expression, external automation/control, modulation elsewhere, or audible behavior.
Every Device record requires exact non-negative `latency_in_samples` and floating
`latency_in_ms`. These values participate in stale-plan equality and final Device evidence, but
`render_path_latency_fully_observed` remains false because the public snapshot does not contain
Delay Compensation, Reduced Latency When Monitoring, Track Delay, complete routing, audio-buffer,
driver, hardware, or acoustic state.
Missing, extended, malformed, or version-incoherent records are protocol errors.
The parser also enforces Song's documented 20–999 BPM domain rather than trusting a bridge payload.
It requires every observed track's clip-slot count to equal the scene count. Score compilation calls
the bridge's explicit `sunny_get_scene_count` adapter and creates scene 0 when it returns zero; results expose
`scenes_created` separately from tracks and clips.

Aggregate project results expose `postconditions.observed`, `track_gates_requested`,
`track_gates_verified`, and one exact-intent/evidence record per Score Part. The planned Track record
contains Part/Track identity, requested Track name, requested `mute`/`solo`, and
`is_frozen = false`, `arm = false`, `implicit_arm = false`, null `group_track_index`, plus
`expected_mixer_enabled = !mute && (!any_project_channel_solo || solo)`. An executing apply fills
the post-snapshot Track name, `has_audio_output`, `has_midi_output`, `is_frozen`, both arm states,
nullable Group Track index, `mute`, `solo`, and `muted_via_solo` observations. Verification requires
identity, exact own gates, audio rather than MIDI output, a disarmed, unfrozen, observed ungrouped
Track, the exact in-range quantised enabled/active/unautomated Track Activator lowering of requested
mute, and no
derived solo-mute when the Mix intent expects the
channel enabled. `disarmed_verified`, `implicitly_disarmed_verified`, and `unfrozen_verified` are
independent from false `monitoring_state_observed` and
`clip_output_not_suppressed_by_monitoring_verified`: the current public Track LOM omits the
monitoring state that Ableton documents as capable of suppressing clips. A recording plan reports
the requests with null observations and zero verified gates.
Group membership has its own observation bit so an unobserved null cannot masquerade as verified
top-level membership. Any final parent index makes the gate incomplete; the parent Track's
mixer/devices/output remain outside this evidence.
Aggregate results separately expose `return_track_gates_requested`,
`return_track_gates_verified`, and one record per generated AuxBus. Each record binds AuxBus ID to
the exact appended Return index and retains requested name, false mute/solo, crossfade assignment
1, Stereo Pan mode 0, and Track Activator 1.0. Executing evaluation fills the final name,
own/derived gates, crossfade assignment, pan mode, and activator parameter facts; all must match,
the activator must be in-range, quantised, enabled/active/unautomated, and `muted_via_solo` must be
false. Recording
plans retain null observations. Return level and scalar pan remain in the generic final mixer
evidence, which separately requires selected-value equality plus an unquantised, enabled, active,
and unautomated domain. `master_track_gates_requested`/`master_track_gates_verified` and their singleton record
similarly require Main Track Activator 1.0, Stereo Pan mode 0, and centered pan 0.0 with in-range,
quantised activator and in-range, unquantised pan evidence, both enabled, active, and unautomated.
The same object exposes `devices_requested`, `devices_verified`, and one final-chain record per
planned native insertion. Each record retains its requested path/name/index/type and the exact
expected mixer-excluding chain size. Executing evaluation fills final Device display/runtime
identity, public type/activity, and observed chain size. Verification rejects missing, extra,
moved, renamed, retyped, or inactive devices. Divergence is successful but warned/incomplete;
malformed snapshot or stored intent evidence remains a protocol error. These are Track-gate and
Device-chain facts. `clips_requested`, `clips_verified`, and the per-Part Clip records additionally
compare final slot-0 name, MIDI role, unlooped marker interval and documented derived `length`,
initial meter, loop flag, and activator state. Clip intent is taken from the Score compiler's stored
property deployments rather than a second duration implementation.
The loop brace is not claimed observed: with the required false loop flag, Ableton defines the
unlooped range by those start/end markers. A looped projection would require separate
`loop_start`/`loop_end` intent and evidence.
`note_batches_requested`/`note_batches_verified` and `notes_requested`/`notes_verified` separately
describe final full-Clip queries. For every inserted batch, the aggregate apply re-queries the exact
nine-field complete note set after the post snapshot and requires both the insertion-returned ID set
and the compiler-retained eight-field property multiset. Well-formed divergence is retained and
warned; malformed evidence fails closed. Unsupported batches remain requested/unverified without
an invalid older-target query. This proves selected sequential final note state, not routing,
parameter durability, tuning interpretation, nonzero signal, playback, or audible equivalence.
`device_parameters_requested`/`device_parameters_verified` and their records separately describe
final arbitrary-device parameter durability. Each logical Timbre or Mix mapping retains its
origin, Part/Track when applicable, source/effect identity, exact device path, requested
name/property/value/range, and execution action. After the structural snapshot proves the containing
device identity, an executing apply performs a selective read-only v17 observation. Verification
requires exact resolved identity, requested value, enabled state, `state = 0`, and
`automation_state = 0`; internal `value` mappings additionally require final `min`/`max` equality.
For `display_value`, Live's returned bounds are retained but range equivalence is deliberately null
because those bounds describe the internal value domain rather than the GUI-visible mapping domain.
Well-formed divergence warns/incompletes; failed lookup is `SendFailed`, and a malformed closed
response is `ProtocolError`. The sequential observer neither repairs automation nor proves sound.
The postcondition object also exposes `song_states_requested`/`song_states_verified` with the final
Song tempo/signature, Scene-0 name/override evidence, and the complete ordered Scene-trigger vector.
It requires `all_scene_launches_quiescent_verified`, meaning only that every sequentially observed
`Scene.is_triggered` value was false; Scene mutation, launch completion, recording preferences,
future stability, playback, and sound remain outside the claim. `cues_requested`/`cues_verified` require
each projected top-level section to have exactly one final CuePoint within the safe adapter's
`1e-7` beat identity window and the requested name. Extra unrelated CuePoints are permitted; an
ambiguous duplicate in the requested window does not verify.
`mixer_properties_requested`/`mixer_properties_verified` and their records retain the last
Score→Mix write for every unique mixer parameter path/property. Final `value` or `display_value`
must match, the target's quantisation class must match the operation, `is_enabled` must be true,
and both DeviceParameter `state` and `automation_state` must be zero. Internal-value requests must
also lie inside the observed finite bounds; display-value requests retain those bounds while
leaving range equivalence null. This catches later fader/pan/send changes, wrong-domain controls,
disabled/inactive controls, and
active/overridden automation; it does not prove the
send's pre/post mode, output route, or audible contribution. The separate Mix send counters retain
that distinction rather than treating final level equality as full send configuration.
Track, Return, and Main gates independently verify their Track Activator as an in-range quantised
control with the same operability facts. The Main gate additionally requires `panning_mode = 0`
and an in-range unquantised, enabled, active, unautomated `panning.value = 0.0`, the neutral lowering
of a source MasterBus that has no mute or pan field.

The native peer recomputes the version-derived feature states and rejects a contradictory profile.
Native LOM path parsing is lossless: it never removes an empty segment or leading separator.
Recording and TCP transports require a `song`-rooted canonical ASCII path and reject a malformed
spelling locally as `not_sent`; the Python peer independently applies the same ASCII index rule.
All compiler-derived Live collection addresses inhabit the protocol's canonical non-negative
signed-index domain. Score Part ordinals, Timbre device positions, and Mix channel/return ranges are
checked before mutation; a value outside that domain returns error 4111
(`TargetAddressUnrepresentable`). Deployment summary counters are 64-bit and are not reused as
target addresses.
The target-independent meter model can express signatures outside Live's current 1–99 numerator
and {1, 2, 4, 8, 16} denominator subset. Such an initial signature returns error 4112
(`TargetValueUnrepresentable`) before target access; later meter changes remain preserved as
unsupported automation. Snapshot parsing and the Remote Script apply the same target domain.
Mix deployment separately calls `sunny_get_return_track_count` for the current Return Track count so new return/send
indices remain correct in a non-empty set. If an AuxBus requires those indices, missing count
evidence is a pre-mutation protocol error rather than an assumed empty set; a graph without an
AuxBus does not make the unnecessary read. Before inserting a mapped Mix effect chain it also calls
`sunny_get_device_count` on the track-like object. The adapter returns the length of its canonical
mixer-excluding insertable-chain view; snapshot device lists and `devices/N` path traversal use the
same domain. This avoids treating the separately exposed mixer child—documented as part of raw
`Track.devices`—as an insertable device. Missing, negative, non-integral, or oversized evidence is
a protocol error, so Sunny never guesses the address of the newly inserted device.
Each created Aux Return is bound to that exact index in `return_track_deployments`, named, set to
`mute = false` and `solo = false`, removed from both crossfader sides with assignment 1, forced to
Stereo Pan mode 0, set to Track Activator 1.0, and given the modeled return level and pan.
Aggregate postconditions export a separate Return gate and require the final name, false
mute/solo/derived-mute, assignment 1, mode 0, and enabled/active/unautomated activator; the generic
mixer evidence independently requires enabled, active, unautomated final level/pan equality.
`master_track_deployment` records Main Activator 1.0, Stereo Pan mode 0, and centered pan 0.0 as
the neutral lowering of absent MasterBus mute/pan intent. Output-route identity and non-pan spatial
fields remain explicit residuals.

### 6.3 Configuration

| Variable | Meaning | Default |
|---|---|---|
| `SUNNY_ABLETON_HOST` | Enable the Live bridge and name its host | unset (offline) |
| `SUNNY_TCP_PORT` | Live bridge port | 9001 |
| `SUNNY_BIND_HOST` | Remote Script listening interface | 127.0.0.1 |

Without `SUNNY_ABLETON_HOST`, theory and document tools remain available while Live-mutating
operations return a clear connection error.

Setting `SUNNY_BIND_HOST` to a non-loopback interface exposes control of the Live session to that
network. This must be deliberate and protected by the host firewall.

## 7. Build and verification

The project requires CMake 3.28 and C++23. Compiler extensions are disabled and warnings are errors.
Dependencies are resolved by `find_package` with pinned FetchContent fallbacks.
Installation exports `sunny::core`, `sunny::render`, and `sunny::infrastructure` through the
versioned `SunnyConfig.cmake` package.

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
PYTHONPATH=python:remote_script:.bin/python uv run pytest tests/python -q
SUNNY_NATIVE_BUILD_DIR=.bin/python uv run python tests/python/run_native_stubtest.py
uv build
```

Formatting is governed by `.clang-format` and `.editorconfig`. Python formatting, linting, and
typing are checked with Ruff and mypy. `mypy.stubtest` additionally compares the authored extension
stub with the module from the current CMake build after disabling any older editable-install import
redirect. The Python build uses scikit-build-core: its platform wheel contains the `sunny` facade
and compiled `sunny_native` extension, while the full CMake install continues to export the C++ SDK
and server.
