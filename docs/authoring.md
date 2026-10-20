# Authoring music with Sunny

Start with an authored project. Its Score owns the musical structure; Timbre and Mix attach to its stable Part identities. Live is a destination for that project.

[Sunny](../README.md) · [Authoring](authoring.md) · [Operation](operations.md) · [Development](development.md) · [Completion](completion.md)

## A first phrase, without Live

Connect your MCP client to `sunny-mcp` without an Ableton route. In a fresh,
empty session, make the following tool calls in order. The IDs below are the
ones returned by that empty session; in an existing workspace use the actual
returned IDs. Replace the save path with an absolute path in an existing
writable directory (inside the container, normally `/data/workspace.sunny.json`).

```json
[
  {"name": "score_create", "arguments": {"title": "First phrase", "total_bars": 1, "parts": [{"name": "Piano", "instrument_type": 47}]}},
  {"name": "create_project", "arguments": {"score_id": 1}},
  {"name": "score_insert_note", "arguments": {"score_id": 1, "part_id": 1, "bar": 1, "offset": {"n": 0, "d": 1}, "pitch": {"letter": "C", "accidental": 0, "octave": 4}, "duration": {"n": 1, "d": 4}, "velocity": 80}},
  {"name": "get_project_json", "arguments": {"score_id": 1}},
  {"name": "workspace_save", "arguments": {"path": "REPLACE_WITH_WRITABLE_ABSOLUTE_PATH"}},
  {"name": "score_export_midi", "arguments": {"score_id": 1, "ppq": 480}}
]
```

`create_project` supplies the owning Timbre and Mix siblings. Inspection returns
their IDs and the current project revision. The exported MIDI contains middle C
for one quarter note: the Score duration `1/4` is a fraction of a whole note,
whereas the MIDI/Live duration is one quarter-note beat. It lasts 480 ticks at
the requested PPQ. Bars are numbered from 1 and offsets from 0.

Edit through the Score/Timbre/Mix tools, inspect the project, then save again.
For example, `score_add_part` on a bound Score creates its siblings in the same
project transaction, and `score_undo`/`score_redo` follow that aggregate history.
`workspace_open` reopens the saved authored state; undo history is in memory.
Neither saving nor exporting this example creates anything in Live.

`instrument_type` is currently a numeric Score instrument enumeration, not a
General MIDI programme number. `47` is Piano, `16` is B-flat Clarinet and `0` is
Violin. Use the defined types in the Score specification; an undefined number
must reject before it can make the workspace unsavable.

## What it does

Sunny represents music as four kinds of document. Each owns one concern:

| Document | Holds | Can be turned into |
|---|---|---|
| Score | parts, notes, rhythm, harmony, dynamics, form | MIDI events, Standard MIDI Files, MusicXML, LilyPond, Live clips |
| Timbre | sound sources, effects, modulation, presets | Live device chains |
| Mix | channels, buses, sends, levels, panning | Live mixer settings |
| Corpus | ingested MIDI and MusicXML works, style statistics | composer and period profiles |

Beneath the documents sits a theory library covering pitch and spelling, scales, chords and
Roman-numeral analysis, voice leading and species counterpoint, rhythm, tuning systems, post-tonal
set theory and acoustics. Pitch and time arithmetic is exact: durations are rational numbers and
overflow is checked, and the engine refuses an invalid request rather than guessing.

The `sunny-mcp` program exposes theory and document workflows as MCP tools and speaks JSON-RPC on
standard input and output. `tools/list` returns the current inventory. Agents can edit measures,
parts, voices, notes, expression, Timbre profiles and Mix effects, and undo coherent project edits.
The engine is also available from Python.

```
AI client ──MCP (stdio)──▶ sunny-mcp ──approved local route──▶ Sunny Remote Script ──▶ Ableton Live
                             │
                             └── theory engine and documents (no Live needed)
```

## Choose the native workflow

For ongoing work use the managed realization tools. The older deployment tools
below create a fresh realization; repeating a new deployment is not an update
of the previously managed objects.

Before native authoring, save the workspace durably and run a fresh `doctor_ableton` in that
same MCP session to establish its original bridge/Set identity. Reconnect or a historical receipt
does not grant new mutation authority. The [operation and recovery rules](operations.md#native-authority-failures-and-persistence) apply to every
native workflow.

Documents reach Live only as a project: a Score, one Timbre profile for each of its parts, and a
Mix graph. `project_plan_to_ableton` records a snapshot of the Live Set and the exact list of
changes without musical mutation. `project_apply_ableton_plan` applies that plan once, refuses if
the observed Set properties have changed in the meantime, and returns a journal of every change it attempted,
including after a partial failure. `project_compile_to_ableton` does both in one call. Values
Live cannot represent, such as a fader above +6 dB, are refused before anything is sent.

## Managed notes and project plans

After `workspace_save`, `project_realization_create` creates one selected Part's managed MIDI
clip and notes with a durable dispatch fence. `project_realization_inspect` reads its actual native
state; `project_realization_update` revises existing attacks and adds or deletes whole Events while
preserving retained native note IDs. These tools require the owning project's current revision. They retain receipts across server
restart and authored undo, detect user drift, and refuse duplicate creation. A lost reply requires
`project_realization_reconcile`, which queries the original token without replaying the mutation.
For several Parts, `project_realization_plan` preflights the selected owning Score, source/effect
controls, static Mixer, Step pan lanes, routing, removed-Part retirement and optional Song settings.
Pass its exact returned plan to `project_realization_apply` with explicit plan and selected-domain
approvals. All phases use the same owning revision and existing per-operation fences. Known
unsupported selections decline before the first fence; a newly inserted device still needs actual
native descriptor admission. A failure stops later phases and returns original attempt identifiers
for reconciliation. Plans also reserve the complete MCP reply within the 16 MiB coordinator
limit; select a smaller aggregate when that reservation or the 4 MiB apply-input limit is exceeded.
Final readback checks the selected physical controls and current cohorts;
unselected domains and audible/DSP equivalence remain explicit limits.
## Clip geometry, envelopes and Song settings

`project_realization_author_mix_lane` also authors a selected Part's Step panning lane when
the native envelope is absent, then checks sampled values. Existing envelopes are preserved.
To revise the whole selected envelope, use `project_realization_preview_mix_lane_replacement`
and `project_realization_replace_mix_lane`. Approval explicitly includes overwriting unsampled
state on that parameter; other parameter envelopes are preserved. Initial authoring and
replacement support up to 64 Step points and report actual samples, without claiming complete
breakpoint capture.
`project_realization_update_geometry` changes the Clip extent and flat meter while retaining
unchanged attacks. `project_realization_update` combines note, length and meter changes under
the final owning revision: it extends before adding later notes and removes tails before
shrinking. Each intermediate projection is fenced and read back separately. Reconcile a lost
reply using its original attempt identifier; the next update resumes from the verified stage.
`project_realization_preview_song_settings` and `project_realization_apply_song_settings`
apply explicitly approved constant tempo and initial meter to the whole Set. Uncertain Set-setting
replies block subsequent managed setters in the active history namespace until reconciliation or
fresh current Set approval. Opening another workspace or applying its backup is refused while
that uncertainty remains; original attempt history remains queryable.
## Devices, Mixer and routing

`project_realization_plan_timbre` reads explicitly selected Drift cutoff and ADSR durations in Hz
and milliseconds. `project_realization_author_timbre` inserts or revises that source using actual
native formatter and parameter readback. Fresh readback of unchanged selected source and active
effect intent returns without repeating device setters. Selected bypassed effects are enabled,
configured and verified, then disabled; their saved configuration is distinguished from current
readable values while bypassed. Unselected sound fields remain reported as unapplied.
`project_realization_plan_effects` and `project_realization_author_effects` cover explicitly
selected native EQ Eight bands and Utility gain/width stages after Drift. Mix input trim uses
a separate Utility, and `set_channel_input_trim` edits its authored dB value. Each native mode
and physical phase is separately fenced and read back; unsupported effects remain unapplied.
`set_channel_flags` edits authored mute/solo values. The static Mixer plan, preview and apply
tools read selected fader, Stereo pan, mute and solo intent from the owning Channel. Fader dB
uses the actual native formatter; programme loudness needs supplied measurement evidence.
Solo requires explicit approval of its Set-wide audible effect. Existing selected automation
is preserved, so apply static pan before authoring its lane. A reopened Mixer needs separate
current-object adoption. `project_realization_retire_part` explicitly mutes a removed Part's
retained Track while preserving its Clip, notes, devices and historical receipts. Mute-only
retirement also retains Solo; the returned Solo state explains when other Tracks remain muted
via solo.
The routing plan, preview and apply tools join owning Aux sends to actual Returns and select
currently advertised output targets. `project_realization_inspect_routing` reads their current
identifiers and attached destinations; inspect again after changing the output type to select
a channel from the newly advertised list. Return Pre/Post policy remains unavailable; conflicting
enabled per-Aux requests decline before any write. A new Return updates all affected Part guards
and requires fresh Mixer approval. Existing flat native Groups can be explicitly adopted against
their complete owning membership; grouped Parts then need their own current Clip approval.
## Reopen and recover

After a Live or bridge restart, `project_realization_preview_adoption` and
`project_realization_adopt` establish explicitly approved current Clip/note authority.
Device authority requires its separate preview/adoption pair. Saved receipts identify history;
fresh native observations and approval establish the current objects. Each result identifies
project domains that have not been applied.
If the Score changed offline before reopening Live, select
`projection_source: "retained_verified_realization"` for the Clip preview and adoption, and
repeat the returned `historical_projection_attempt` when approving adoption. This establishes
the current objects against their last verified musical baseline; the next update applies the
current Score. If physical intent also changed offline, use
`device_projection_source: "retained_verified_realization"` for the separate device preview/adoption
and repeat its returned `device_history_attempt`. This reads the saved finite physical targets and
original tolerances from verified history, then checks them against current native objects without
setters. The current owning source/effect writers apply the newly authored values after adoption.
The current logical device keys, classes and order must still agree; historical receipts never
restore old native identities.
For removed Parts, Clip recovery uses the last verified historical note/geometry projection;
separate mute-only Mixer adoption permits retirement without recreating the Score Part.

## Standalone clips and Max

A few quick tools (`create_progression_clip`, `apply_euclidean_rhythm`, `apply_arpeggio`) write a
single clip into an empty clip slot without a project. Save the workspace durably before using
them. Every create, undo and redo retains the original native object authority and writes a
durable attempt before sending it. A lost response remains indeterminate; inspect
`ordinary_clip_history` and use `ordinary_clip_reconcile` to query the original token. Neither
reconnect nor reconciliation repeats a mutation. Acknowledged creations can be undone with
`undo_ableton_operation` while their original native objects and unchanged content remain available.
Restart preserves query history; it does not restore the in-memory undo stack.

A separate `max-package/` builds five Max externals (`sunny.lfo~`, `sunny.adsr~`, `sunny.hold~`,
`sunny.clock~`, `sunny.events`) from the same render code. See the [Max package guide](https://github.com/sunyaisblank/Sunny/blob/lifecycle-distribution/max-package/readme.md).

