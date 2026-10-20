# What complete means for Sunny

Sunny is complete when an agent can author a coherent musical project, inspect
and revise it, save and reopen it, and realize the supported intent in Ableton
without losing the music or altering unrelated work. That entire workflow must
be understandable to a person maintaining or operating it.

[Sunny](../README.md) · [Authoring](authoring.md) · [Operation](operations.md) · [Development](development.md) · [Qualification](qualification.md)

The accepted programme is recorded in [issue #1](https://github.com/sunyaisblank/Sunny/issues/1)
and its [operating lifecycle extension](https://github.com/sunyaisblank/Sunny/issues/36).
Passing a build, adding a verifier, or preparing a host kit completes only that
part of the programme. Real Ableton execution is the final acceptance stage.

## A finite finish line

| Required outcome | Sufficient acceptance |
|---|---|
| Compose, inspect and revise | A multipart Score with voices, structural edits and expression stays coherent with its Timbre and Mix. Failed edits preserve documents, identities and history; undo/redo restores the relationships. |
| Preserve musical meaning | Independently derived pitch, sounding intervals, rational timing, ties, tuplets and transposition survive the supported analysis and export paths. Target losses are explicit. |
| Save and recover the project | Every admitted authored value can be saved and reopened. Process/container replacement, corruption, interrupted publication, backup recovery and competing writers preserve the last valid workspace and its separate native history. |
| Give useful corpus evidence | Supported ingestion and motif, rhythm, form and context queries return traceable passages, methods, denominators and availability. Missing evidence is reported without invented conclusions. |
| Realize and revise supported intent | Managed author/revise/readback joins the current owning project revision to actual native objects. Repeated work does not duplicate them. Drift, changed objects, cancellation and lost replies have explicit recovery outcomes. Unrelated Set content is preserved. |
| Deliver and operate one product | The paired image, bridge, operators, guides and configuration agree. Install, ordinary use, diagnosis, update, failed-update recovery, compatible rollback, stopped-writer backup/restore and uninstall work under the accepted Windows/Docker profile. |
| Remain comprehensible | A new reader can find a first musical workflow, the owning source modules, routine checks and recovery instructions. Maintained tests have a distinct purpose; generated builds and historical evidence do not masquerade as product source. |
| Finish on actual hosts | The exact artifacts and hosts pass all applicable original qualification groups, complete musical revisions and native save/reopen, with independent visual/audio observations and the operating lifecycle. Repair discovered failures, check the final revision, then reconcile and merge main. |

These gates preserve the original scope. A selected-domain `complete` result
does not complete the product. Neither a passing smoke test nor a successful
doctor closes the full [native qualification obligation](https://github.com/sunyaisblank/Sunny/issues/22).
The [23-group inventory](https://github.com/sunyaisblank/Sunny/blob/lifecycle-distribution/tools/live_qualification/obligations.json) belongs to
that final stage; the packaged copy is `operator/live_qualification/obligations.json`.

## The supported musical boundary

The primary operating target is Windows 11, Docker Desktop Linux/amd64 and
native Live 12.4.5 Suite. Live 12.3 candidates and limited Live 11 operations
remain distinct compatibility claims with their own evidence requirements.
No exact combination is currently fully qualified. An unavailable older API
does not prove absent musical content.

The finite managed native subset includes MIDI note/event and Clip geometry
revisions, constant Song tempo and initial meter, selected Drift cutoff/ADSR,
EQ Eight and Utility stages, static Mixer controls, selected advertised routing,
Aux sends and Step pan envelopes. Its detailed admission and recovery contracts
are in the authoring guide and formal specifications. A numeric control match
is evidence for that control, within its stated tolerance.

The broader IR can express more than this native subset. Unsupported mandatory
intent must decline before mutation; unselected or unapplied intent must remain
visible. Native Group creation/regrouping, Return processing and Pre/Post policy,
general device/modulation/automation translation, arbitrary plugin state and
later tempo events are not silently included by a successful selected plan.
Changing this boundary is a product decision, not a way to make a failing gate
pass. The existing native obligations must retain their original applicability.

Sunny does not provide its own audio engine, recorder, mastering analysis or
graphical notation editor. Audio measurements must be supplied as evidence;
musical taste cannot be inferred from parameter readback. Max externals have
build/package evidence but still require actual Max/Max for Live loading.
Live's current-scale setting is readable but is not written. No real Live
end-to-end run has been completed.
Those limits do not excuse a missing promised workflow inside Sunny's boundary.

## What related projects suggest

These are design comparisons, not additional acceptance requirements or claims
that Sunny should implement every feature of another application.

| Project | Relevant strength | What Sunny should take from it |
|---|---|---|
| [music21](https://music21.org/music21docs/about/what.html) | Symbolic composition, notation and analysis with small executable examples and an included corpus | Lead with useful musical tasks and explain what each analysis can establish. A catalogue of theory functions is insufficient onboarding. |
| [MuseScore](https://handbook.musescore.org/basics/parts) | Instrument parts remain connected to a common score while their presentation can differ | Make ownership and structural editing predictable. Sunny's project must preserve Score/Timbre/Mix relationships through edits, undo and reopen. |
| [Ardour](https://manual.ardour.org/working-with-sessions/session-templates/) | Named session/track templates retain track, bus, plugin and routing setup for reuse | Make the normal session lifecycle discoverable. Reusable project setup and a readable account of what has changed matter more than more low-level controls. Templates are a possible later feature, not a newly imposed gate. |
| [TidalCycles](https://tidalcycles.org/docs/innards/what_is_a_pattern/) | Patterns compose through a clear time-to-events model with rational time and explicit event spans | Explain Sunny's timing units and sounding-versus-attack semantics consistently. Keep musical transformations composable and independently checkable. |

Sunny's useful distinction is the connection between checked symbolic music,
production intent and durable agent-driven native editing. The remaining design
work should strengthen that connection: understandable entry points, consistent
admission, coherent revisions and reliable recovery. More analysis machinery or
an ever-growing tool count is not a finish line.

## Order of remaining work

First resolve demonstrated software defects and the public workflow/readability
gaps. Reuse valid existing evidence; rerun affected checks when their source or
contract changes. Keep the frozen old release distinct from a new source revision.
Historical logs and generated builds can be retained outside the checkout after
their provenance and consumers have been checked.

Then freeze and verify the integrated candidate. Finally perform the approved
two-machine setup and [native qualification](qualification.md), including the
original recovery, save/reopen, UI/audio and RDP obligations. Registry destination,
visibility and publication permission remain separate decisions; no publication
is authorized by the existence of a local image or an offline ZIP.
