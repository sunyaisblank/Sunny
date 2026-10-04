# Sunny Managed Live Realization Foundation

**Managed receipt schema:** 1  
**Bridge protocol:** 46, unreleased coordinated contract  
**Status:** Operational offline foundation; no product MCP admission or host qualification

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
object through an index or tag.

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
relative to that same Track. `lane` has exactly `parameter`, `interpolation:"step"`,
`clip_end`, and `points:[{time,value}]`. Points start at zero, increase strictly after
floating conversion, and end before `clip_end`; positive step durations cover adjacent
intervals and the final marker interval. Values are actual Live internal units.
Device selectors, physical-unit conversions, curves, Arrangement clips and foreign
objects are unsupported. The existing separate `sunny_get_step_envelope` operation
samples actual native envelope state and is classified read-only.

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
This foundation does not implement that product store. `execute_managed_operation`
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

Successful create/replace/envelope journals require `native_mutation_started=true`;
read-only rebind requires false. Result logical tags must match the original request.
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
bridge must continue to decline recovery while complete content verification is
unavailable. Those checks must not be reported as complete native aggregate deployment.

Issues30 and31 remain open for owned device insertion/parameter mapping, full project
integration, durable workspace receipt wiring, safe selective updates and complete
saved/reopened ownership recovery. Native host qualification remains in issue22.
