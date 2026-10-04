# Sunny Managed Live Realization

**Managed receipt schema:** 1  
**Bridge protocol:** 46, unreleased coordinated contract  
**Status:** Durable selected-Part notes, physical Drift controls, Step panning and explicit current-object adoption; host qualification pending

Live12.4 is the primary product target, with compatible Live12.3 behavior retained.
The exact host patch, edition and operating system remain final qualification facts.

## 1. Native identities and preservation boundary

`ManagedRegistry` lives on the Remote Script Control Surface. All dispatch and
write authorization use the existing Live main-thread scheduling path. A binding
retains the actual native Track, ClipSlot and Clip identities, not ownership inferred
from their names or current indices. Indices are resolved again from these identities
for each observation or authorized lane; track or scene reordering does not transfer
ownership to another object. Clip/Slot/Track canonical parents must also agree.

A bridge instance and Live Set document token are independent UUIDs. TCP reconnects
retain the registry and operation journal. A different Song identity resets the
bindings, document token and journal. A bridge/host restart creates a new instance
and document token. An old-token operation returns `unknown_epoch`, which never
proves that an earlier mutation did not execute.

Logical project and binding keys use ASCII letters, digits, underscore and hyphen,
with length1..64. The created tags are `Sunny|PROJECT|BINDING|track` and
`Sunny|PROJECT|BINDING|clip`. Existing matching tags prevent duplicate creation;
tags alone never confer ownership. A fresh registry cannot authorize an old native
object through an index or tag without a separately approved current-object preview.

The finite structural manifest includes the complete modern eight-field semantic
note population, Clip marker/loop/launch/groove/runtime scalars, Track runtime and
I/O state, selected routing identities, internal mixer parameter domains/values/states,
and explicit empty devices, other occupied Session slots, Arrangement clips and take
lanes. Note IDs are excluded from the persistable semantic manifest because host
reopen may change their identities. Actual insertion IDs remain part of the underlying
note adapter's acknowledgement contract. Scene/Track indices are excluded from the
content fingerprint; observed current locators are returned separately.

This is **not complete preservation coverage**. Public note dictionaries do not expose
per-note MPE expression. Follow Action settings are not observed. Full native envelope
breakpoint populations are unavailable through bounded `value_at_time` samples.
`mpe_note_expression_state_observed` and `follow_actions_state_observed` are false.
`structural_boundary_complete` describes the finite declared observation;
`content_boundary_complete` remains false. Named `MpeExpressionUnavailable` and
`FollowActionsUnavailable` reasons are always retained. Destructive replacement and
restart rebind decline before mutation, even when all finite structural fields match.
They cannot erase an edit confined to an unobservable native field.

A first isolated mixer Step lane on retained owned identities remains useful: it
preserves the existing Clip and its unknown fields. Admission uses the finite structural
boundary, exact unchanged fingerprint, idle generated `[0,clip_end)` nonlooping MIDI
Session Clip, unarmed Track, same-track real DeviceParameter, active/enabled continuous
internal domain, and no overridden automation. The authorizer is active only inside
that tokened managed lane and matches the exact resolved Track/Clip/Parameter identities.
Generic envelope authoring is denied outside this context, including generic writes to
an otherwise owned Track. Subsequent complete update/recovery of envelope content is
unavailable; samples are evidence only for their queried times.

## 2. Closed versioned operations

Every operation is a `call` on `song`. Context has no arguments. All other calls have
one exact dictionary argument; unknown fields, aliases, invalid keys, Boolean numeric
substitutes and nonfinite controls are rejected by both native and Python protocol
validators before dispatch.

| Operation | Exact payload fields beyond common fields | Behavior |
| --- | --- | --- |
| `sunny_managed_context` | No payload | Read-only schema1, bridge instance and document token |
| `sunny_managed_operation` | `document_token`, `operation_id` only | Read-only retained operation lookup |
| `sunny_managed_observe` | `document_token`, `project_key`, `binding_key` only | Read-only native identity/content observation; missing/partial handles remain explicit |
| `sunny_managed_create_clip` | `clip_end`, `signature_numerator`, `signature_denominator`, `notes` | Append one native MIDI Track, create its first Session Clip, write generated note/scalar state, observe actual result |
| `sunny_managed_replace_clip` | Create fields plus `expected_content_fingerprint` | Default-denied while complete preservation coverage is unavailable |
| `sunny_managed_rebind` | `expected_manifest` | Require unique tags plus exact complete persisted native content; currently RecoveryUnavailable for unobserved MPE/Follow Actions |
| `sunny_managed_author_envelope` | `expected_content_fingerprint`, `lane` | Author the exact guarded mixer Step lane; acknowledge native calls separately from sampled readback |
| `sunny_managed_sample_envelope` | `document_token`, `project_key`, `binding_key`, `parameter`, `sample_times` only | Read-only actual envelope samples on retained identities |
| `sunny_managed_update_notes` | `expected_content_fingerprint`, `changes` | Revise retained native note IDs after full-population and collision preflight |
| `sunny_managed_revise_note_population` | `expected_content_fingerprint`, `changes`, `deletions`, `additions` | Selectively delete/add attacks and revise retained IDs without reconstruction |
| `sunny_managed_preview_adoption` | `document_token`, `project_key`, `binding_key`, `selector` only | Read-only exact current Clip/Track/IDs preview; no authority |
| `sunny_managed_adopt_clip` | `preview_token`, `preview_fingerprint`, `explicit_adoption:true` | Fenced fresh current-object Clip/note authority; no native setters |
| `sunny_managed_insert_device` / `sunny_managed_update_device_parameters` | `expected_content_fingerprint`, `expected_device_identity_fingerprint`, `device_key`, `device`, `physical_intents` | Resolve all selected actual native controls before setters; retain native devices on partial failure |
| `sunny_managed_preview_devices` | `document_token`, `project_key`, `binding_key`, `expected_content_fingerprint`, `devices` only | Read-only full finite current device-chain preview; empty-chain insertion grant requires separate approval |
| `sunny_managed_adopt_devices` | `preview_token`, `approved_preview` | Fenced fresh current-device controls/append authority; no native setters |

Mutation common fields are `document_token`, `operation_id`, `project_key`, and
`binding_key`. `clip_end` is finite positive Live quarter-note beats, signature
numerator1..99 and denominator one of1/2/4/8/16. Empty notes are allowed. Nonempty
notes use the existing closed deterministic eight-field `add_new_notes` shape;
probability1.0/velocity deviation0.0 admission remains unchanged. Every note starts
in `[0,clip_end)`; durations can cross the marker as existing Score projection permits.

Creation requires existing Scene0; it never creates or renames global Scenes. The new
Track is appended and its actual identity is derived from before/after native Track
membership, independently of a native method return value. Its Track/Clip tags and
unarmed/generated scalar state are installed; MIDI specifications are passed through
the existing note adapter. Actual queried values populate the acknowledgement's
manifest. `observed_notes_match_request` and
`observed_clip_properties_match_request` report independent value comparisons, not
echoed intent. An acknowledgement can retain unavailable boundaries or a value
mismatch; it is not a complete realization or playback assertion.

The first managed envelope subset admits only `volume`, `panning`, or `send` selectors
relative to that same Track, and requires the selected envelope to be absent. Existing envelopes
on other parameters and unknown Clip fields are preserved. `lane` has exactly `parameter`, `interpolation:"step"`,
`clip_end`, and `points:[{time,value}]`. Points start at zero, increase strictly after
floating conversion, and end before `clip_end`; positive step durations cover adjacent
intervals and the final marker interval. Values are actual Live internal units.
Device selectors, physical-unit conversions, curves, Arrangement clips and foreign
objects are unsupported. `sunny_managed_sample_envelope` resolves retained Track/Clip/Parameter identities in one
main-thread call and returns actual samples without refreshing any mutation guard. The generic
index-based sampler remains read-only but does not establish managed ownership.

## 3. Retained journal and uncertain outcomes

Before any native mutation, the registry reserves a journal entry containing the
operation token, exact request and canonical request fingerprint. An identical token
and payload returns the retained actual acknowledgement or failure record. Reusing a
token with different payload is rejected. The journal admits at most4096 operations
and never evicts a token to permit duplicate execution. A new Live Set/bridge epoch
is explicit, not a journal reset that proves old operations absent.

Outcome states include `acknowledged`, `declined` before native writes, and
`indeterminate` once a native call could have changed the host. A failure after Track
or Clip creation retains any uniquely observed partial native identities. There is no
compensating deletion. An identical failed token reports that failure and never reruns
the mutation; a different create token for an already retained binding also declines.
Partial binding observation reports known current native locators when available,
without claiming a complete Clip or recovery authority.

Native APIs prepare a serializable receipt before sending. The caller must persist
that prepared receipt before execution, then persist its returned delivery/outcome
record; product integration owns that durable storage. Before every possible native
send, it must also durably install a may-have-sent dispatch fence. A process can die
after sending but before saving its result, so a disk-restored Prepared receipt alone
never proves that the mutation was not sent. Recovered fenced attempts are query-only;
authored workspace rollback, undo and backup recovery must not erase native history.
`RealizationStore` implements that product store with a lifetime writer lock and strict ledger
validation. The public workspace save establishes its persistent namespace and location;
`project_realization_create` and `project_realization_update` consume a single dispatch permit
after durable fencing. Workspace schema2 retains that location. Authored import, undo and backup
recovery never erase the operational ledger. `execute_managed_operation`
sends exactly once. Only a prepared or proven `NotSent` receipt allows an explicit
same-token send. A sent uncertain, partial, foreign or malformed response remains
indeterminate and cannot be automatically replayed. Transport exceptions conservatively
retain uncertain delivery rather than claiming the request stayed local.

`reconcile_managed_operation` sends only the read-only operation lookup. It compares
retained operation identity, name and exact payload against the original request.
A failed read preserves original mutation delivery. `unknown_operation` in the same
epoch and `unknown_epoch` across changed epochs remain unknown, never safe-retry or
absence proofs. Persisted receipts retain the original version46 request, context,
delivery, outcome, journal and error. Schema/version/type mismatches are rejected.
Persisted binding receipts retain tags, logical keys and the actual observed manifest;
they do not contain usable native pointers or transfer ownership by index.

Successful native mutations require `native_mutation_started=true`;
rebind and the two explicit current-object adoption operations require false. Result logical tags must match the original request.
Request and actual-content fingerprints use shared bounded typed SHA256 encoding,
preserving JSON scalar types, IEEE754 signed zero and UTF8 text independently of
JSON float spelling. Native receipt validation recomputes note and Clip match flags
from actual values. Truthful mismatch evidence remains admissible; malformed or
contradictory evidence never authorizes retry or erases a historical terminal receipt.

`managed_clip_projection` extracts an already recorded compiler Clip's quarter-note
geometry, signature and deterministic note dictionaries. It does not implement another
musical timing conversion. Requests outside that selected Clip and its generated logical
name/blanket-envelope-clear requests remain explicitly unapplied; tempo, Scenes, cues,
other Parts, instruments, effects and routing are not silently claimed as realized.
Unsupported selected-Clip semantics reject projection.

## 4. Source evidence and host obligations

The current [public LOM](https://docs.cycling74.com/apiref/lom/) reference describes
Live12.4.5. [Clip notes](https://docs.cycling74.com/apiref/lom/clip/) document full
`get_all_notes_extended` readback since11.1;11.0 uses the finite all-pitch range
`[0,clip_end)` and cannot establish whole-Clip population. Take-lane API absence is
unknown content rather than an empty collection. Existing Live11 creation is retained
with limited evidence; no legacy destructive update/rebind is claimed.

The source-observed native Python envelope methods are separate from the public Max
Clip LOM. The pinned mirror
[e83d5192f321b24eb9daab843ac49a2d95d862b1](https://github.com/gluon/AbletonLive12_MIDIRemoteScripts/tree/e83d5192f321b24eb9daab843ac49a2d95d862b1)
uses actual DeviceParameter objects with `automation_envelope`,
`create_automation_envelope`, `insert_step` and `value_at_time` in
[`pushbase/automation_component.py`](https://github.com/gluon/AbletonLive12_MIDIRemoteScripts/blob/e83d5192f321b24eb9daab843ac49a2d95d862b1/pushbase/automation_component.py).
Source file compilation dates and this repository commit do not establish every
Live11/12.3/12.4 Python ABI. The registry does not assume general `display_value`
availability or device licensing from the public12.4.5 reference.

Offline tests qualify protocol, identities, model behavior and fault bookkeeping.
They cannot qualify a Live host, saved/reopened envelope persistence, GUI units,
licensing, sound or complete native preservation. Final host probes must record exact
Live version/edition/OS and bridge commit, actual method/domain availability, native
Track/Clip creation and note readback, dropped-reply reconciliation without duplicates,
reconnect/Live Set reset/restart behavior, track/scene reorder and known/unknown user
drift. They must separately author a managed Step lane, independently sample its
native values, save/reopen the Set and inspect native envelope persistence; a new
bridge requires fresh explicit current-object adoption for in-place revision. Complete native
content verification and destructive recovery remain unavailable. Those checks must not be reported as complete native aggregate deployment.

The selected Part MCP workflow resolves a current owning project revision and uses the existing
Score compiler. Attack addresses retain the source Event identity and note ordinal; tied
continuations retain their original attack. The desired projection and addresses are fingerprinted
and stored before dispatch. A subsequent note update uniquely associates each old desired note
with the actual retained native note ID, and refuses ambiguous association. Creation repeated
after an acknowledgement reports an existing binding; an unresolved attempt requires query-only
reconciliation. Pure owner/revision validation and retained uncertainty selection precede bridge
profile admission, so a disconnect cannot hide the original attempt token. Offline acknowledged
creation inspection explicitly leaves current desired-content comparison unavailable. A proven no-effect decline preserves the last acknowledged binding for another
explicit candidate. Native readback mismatch is reported as verification failure even when Live
acknowledged its calls.

Existing-ID changes admit pitch, start, duration, attack/release velocity and mute. They require
the unchanged finite content and note-ID fingerprints, full native population, an idle unarmed
nonlooping MIDI Session Clip, and unchanged clip length/meter. Geometry preflight checks the
proposed entire population for finite endpoints and same-pitch half-open overlap before native
setters. Changed destinations must also avoid other baseline notes: same-pitch start swaps remain
unavailable until host batch atomicity is established, even when the final intervals would not overlap. The native adapter obtains detached MidiNote snapshots and passes them to
`apply_note_modifications`; it does not substitute note dictionaries or new specifications.
Unchanged IDs and untouched semantic fields are independently read back. Opaque MPE expression,
Follow Actions and envelopes are preserved in place rather than claimed as observed.

`project_realization_inspect` reports retained history and a separate current observation; it
never refreshes a mutation guard. `project_realization_reconcile` looks up the exact original
operation token and appends actual evidence without replay. An unchanged desired-note update
also performs a fresh read-only comparison before reporting success. Attempt IDs allow inspection
of history for retired authored Parts. No tool claims whole-project realization from this subset.

`project_realization_author_mix_lane` admits only the selected Part's owning
`channels[PartId].spatial.pan` lane with Step interpolation and the observed stereo `[-1,+1]`
domain. Score whole-note rational positions convert explicitly to Live quarter-note time by
multiplication by four. Readback compares each Step start and representable interval midpoint
with its authored value at absolute tolerance `1e-6`. This proves only those samples; it never
claims complete envelope population. A repeat preserves any existing selected envelope, including
unsampled user edits. Linear/curve interpolation and complete envelope revision remain unavailable.

## 5. Population, device controls and current-object adoption

`project_realization_update` preserves existing Event/ordinal attack associations and permits
whole Event addition/deletion. An existing Event's chord cardinality cannot change in place:
`UnsupportedChordCardinalityRevision` prevents transferring unknown expression to another
ordinal. Explicit deletion followed by a new Event is a separate lifecycle. Selective native
remove-by-ID, detached existing-note application and new specification insertion run in that
order, with full actual ID/value readback after each phase. An intermediate mismatch stops
later phases. Returned added IDs remain may-have-created evidence immediately; new operational
associations require unique actual value-to-key matching, independently of return order. The
fourteen-field population supplement independently verifies changes, absence, additions,
untouched notes, retained IDs and cardinality. No phase compensates or replays after uncertainty.

The finite bridge frame limit is 16 MiB. Admission includes the immutable request, outer
success/value envelope, complete prospective notes/IDs, before/after evidence and optional device
state. Every future finite float reserves 32 ASCII bytes and native IDs reserve their signed
int32 width; escaped text and wire separators are counted. Known note evidence is budgeted before
Track creation, and actual new Track/Clip metadata is checked before note insertion. Failure
while learning native metadata retains any created objects as partial evidence. Existing-ID,
population, creation and adoption builders return `ManagedReplyCapacityExceeded` (4113) before
the product fence when their conservative bound exceeds capacity. This is a byte admission
boundary, not a guarantee that every population below 65536 fits. A direct request whose echoed
failure journal itself cannot fit is rejected before reservation with a compact error and no
native calls; it is not journal-confirmed `declined`. Native error text is bounded to 1024
characters. The conservative bound can decline a request whose eventual actual reply would fit.

`project_realization_author_timbre` applies the explicit physical selections returned by
`project_realization_plan_timbre`: Drift cutoff in Hz and ADSR stage durations in milliseconds.
The original legacy rendering mappings stay unchanged. A stable source key is derived from the
owning profile ID. Insertion requires an actual empty retained chain and a separately retained
Track append grant; revision requires the exact retained source Device/parameter cohort. All
native formatter candidates resolve before parameter setters. Actual formatted readback must
meet the requested tolerance; untouched parameters, modes and note IDs remain guarded. Insertion
can create a device before its actual descriptors or availability can be fully resolved. Late
failure retains that object and blocks replay. Knob agreement does not qualify DSP equivalence;
unselected profile leaves and effects remain explicit residuals.

The native helper supports Drift, Utility and EQ Eight, at most 16 flat devices, 32 physical
controls per device and 512 parameter descriptors. The current owning public device path admits
an empty or source-only Drift chain. Additional effect-chain authoring requires its owning
mapping and order contract; the helper's inventory alone is not an end-to-end product claim.
Full device capture observes a finite floating-point default only for continuous parameters.
For quantized parameters it records `default_value:null` without accessing that getter, as
required by the [DeviceParameter contract](https://docs.cycling74.com/apiref/lom/deviceparameter/).
The native consumer requires this distinction rather than manufacturing a default.
Capture metadata never confers append authority. Only original managed Track creation or fenced
explicit current-device adoption grants it; a clean same-handle recovery may retain an existing
grant. Ordinary note acknowledgement cannot upgrade preserve-only device authority.

`project_realization_preview_adoption` selects current Sunny-tagged Track/Slot/Clip identities
and captures their actual notes/IDs, Set metadata and device cohort. Tags select candidates;
they are not proof of historical identity. The preview reports whether the current compiled
owning Score and unambiguous attack associations match, and grants no authority.
`project_realization_adopt` requires literal approval of that exact preview and durably fences
the fresh authority transition. It grants existing-note updates, population revision and absent
mixer Step lanes while preserving unknown MPE, Follow Actions, envelopes and devices. Later
current-state/epoch/handle changes decline. A successful fenced adoption establishes a new
current authority boundary for that Part: earlier uncertainty stays saved/queryable and is never
an absence or retry proof; uncertainty after the new boundary still blocks dependent writes.

`project_realization_preview_device_adoption` and `project_realization_adopt_devices` grant
separate current-device authority. Existing selected controls must already match the owning
physical targets at preview and approval; these operations never repair mismatches with setters.
An approved empty chain grants source insertion and owned-effect append on the exact current
Track without claiming formatter evidence for devices not yet inserted. A current known-chain
grant does not restore historical native identity. Unknown plugins/racks remain preserve-only.
After reopening a real Set, these in-place paths require final host qualification of actual
current identities, IDs, parameter/mode state and persistence. Native host qualification remains
in issue22; essential geometry, routing and further automation integration keep issues30/31 open.
