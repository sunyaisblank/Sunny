# Sunny Workspace Format

## 1. Envelope and identities

The `sunny-workspace` envelope version 2 persists the authored workspace. Required fields are
`format`, `version`, `scores`, `timbre_profiles`, `mix_graphs`, `preset_library`, `corpus`,
`projects`, `namespace_history`, `counters`, and `native_realization`. Unknown envelope fields and
duplicate JSON fields are refused. Version 1 migrates through the same strict authored-state
reader and receives a fresh native workspace namespace. Both versions require the complete
namespace history; it is not inferred from absent active documents.

The three document-store arrays contain `{id, document}` records. A positive wrapper ID equals
the embedded root ID; store IDs are unique within their store. Nested documents use the canonical
Score, Timbre, Mix, and Corpus codecs and their declared schema versions. Supported legacy child
versions migrate through those readers. The envelope does not duplicate musical algorithms or
freeze child schema constants. Saving verifies the actual textual codec round trip before I/O.

`preset_library` stores the shared preset codec. Preset identities are positive and unique,
descriptors satisfy the Timbre domain, and parameter-state values are finite. A preset's paths
are checked against the destination profile when loading that preset. Presets embedded in a
profile retain their local scope.

Each `projects` record stores `score_id`, `mix_graph_id`, `timbre_profile_ids`, `revision`,
`history_capacity`, and sorted unique positive `reserved_channel_ids`. The owning Score identity
also identifies the project. Membership is exclusive and exact cross-document Part correspondence,
typed references, and the shared-preset morph closure must validate. Channel reservations cover
represented channels and retain retired identities. Score's own schema persists reservations for
Events, Parts, Sections, Tuplets, and BeamGroups. Reservations admit unused holes below high
imported identities rather than reserving every smaller integer.

`namespace_history` has three arrays sorted by positive namespace identity: `scores`,
`mix_graphs`, and `projects`. Each Score record stores `score_id`, a positive `version_floor`,
and an `identities` object containing sorted unique positive `events`, `parts`, `sections`,
`tuplets`, and `beams` arrays. Each graph record stores `mix_graph_id` and sorted unique positive
`channel_ids`; each project record stores `score_id` and a positive `revision_floor`. An active
Score's version equals its floor, and its represented and retired typed identities equal its
history reservations. An active project's revision equals its floor and its Channel reservations
equal the graph history. Explicit outer reservations also survive migration of a legacy child
schema which cannot represent those reservations itself.

Inactive namespaces retain their histories without active documents or native object pointers.
An empty authored workspace can therefore retain every identity and publication floor already
exposed. Local typed reservations may contain `UINT64_MAX` while preserving unused smaller
holes. A maximum version or revision floor prevents a later activation which would require an
increment.

`counters` stores all existing document, preset, effect, bus, reference, composer, work, and
deployment-plan allocator high-water values. Global counters are positive and exceed represented
allocated identities, including inactive Score and graph namespace keys; an exhausted global
domain is reported without wrapping. Local effect,
Part, and bus identity duplication across different documents is permitted. Corpus structural
admission uses `blocks_corpus_load`; evidence-quality diagnostics alone do not prevent reload.

## 2. Publication and import

Opening parses and validates a private owning candidate before replacing the authored stores.
The existing session `shared_ptr` objects remain stable, so handlers registered before open
continue to resolve the restored documents. Publication occurs under the server's serial request
boundary and uses nonthrowing swaps. A rejected input leaves documents, bindings, counters,
history, and deployment plans unchanged.

Open explicitly replaces authored workspace state. Import merges complete valid snapshots and
refuses store, composer, work, or shared-preset identity collisions; it never invents remapped
identities. Both operations preserve the current session's allocator high-water values. Saving,
opening, and importing collect active reservations and publication floors and merge them with
retained namespace history in a private candidate. Opening an older same-ID Score retains all
observed typed identities even if another workspace removed the Score in between. Reopening the
same Mix graph also retains observed Channel reservations, including when its owning Score changes.
Saving an empty active workspace persists inactive histories, so a later restart cannot erase
those reservations. The history maps publish in the same nonthrowing swap as the document stores.
Runtime undo/redo snapshots and deployment
plans are ephemeral and are cleared after successful open/import. Plan IDs never rewind, and Live
object identities and transport state are not deserialized.

Replacing or recovering an already encountered Score namespace publishes a version greater than
both the saved version and retained floor. A same-ID project likewise advances beyond both
revisions. A namespace restored only as inactive history counts as encountered for later activation.
Exhaustion rejects the private candidate before publication. A fresh process restores active saved
versions as represented. Import advances an existing Score's version whenever incoming inactive
history adds typed reservations or establishes a higher floor. A changed owned Score or additional
Channel reservations also advances its owning project revision. Unchanged unrelated documents and
projects preserve their versions. All required increments are checked before any publication. Thus
an in-process open cannot reuse an observed version for older content, while restart retains the
persisted document identity and configuration.

## 3. Durable replacement and recovery

`workspace_save` first produces a validated canonical text snapshot. A supported, valid previous
main file is preserved as `.bak`; a corrupt or unsupported previous main never overwrites the
last valid backup. A version-1 previous main is backed up as a canonical version-2 envelope
with its old authored content and the current native namespace and history location. This prevents
backup recovery from inventing another namespace and losing existing dispatch fences. Each replacement creates an exclusive temporary file in the same directory,
checks complete writes, synchronizes the file, atomically renames it over the destination, and
synchronizes the directory on POSIX platforms. Temporary files are removed after pre-replacement
failures. The platform contract follows the [POSIX filesystem synchronization rationale](https://pubs.opengroup.org/onlinepubs/9799919799/xrat/V4_xbd_chap01.html).

Save reports `committed` separately from `durability_confirmed`. Failure before replacement
preserves the old main bytes. Failure after replacement reports the new visible file as committed
and durability as unconfirmed. The Windows implementation uses checked Win32 writes,
`FlushFileBuffers`, and write-through replacement; it reports directory durability as unconfirmed
because it cannot establish the POSIX directory synchronization proof.

`workspace_recover_backup` explicitly validates `.bak` and previews it by default. `apply=true`
loads that supported snapshot transactionally without changing the main file. A corrupt main is
never silently replaced by fallback state. A subsequent explicit `workspace_save` repairs the
main path. External sample and preset paths are preserved as authored references; this format
does not collect the referenced assets.

## 4. Native realization history

`native_realization` contains exactly `workspace_namespace`, a 32-character lowercase hexadecimal
identity, and `history_base_directory`, either null or a nonempty filesystem path. Native history
is operational state outside authored project undo and backup snapshots. An initial public
`workspace_save` creates its namespace directory beside the saved workspace and publishes the
history location only after saving the authored envelope. Native mutation admission requires a
durably saved workspace namespace and the existing, writable history store. A migrated version-1
workspace must be saved before native authoring. Missing or corrupt existing history is refused;
opening a workspace never silently initializes replacement history.

The history directory is `history_base_directory/workspace_namespace`, with `ledger.json` and a
lifetime single-writer lock. The ledger retains immutable desired projections, stable Score attack
addresses, prepared requests, dispatch order, may-have-sent fences, actual receipts and observed
bindings. Its checked temporary-write, file-sync, rename and directory-sync sequence completes
before a one-use dispatch permit is issued. Reloading the ledger never recreates a dispatch permit.
A crash after native mutation but before receipt persistence therefore leaves a query-only attempt.
Uncertain or partial outcomes block another mutation of that binding until reconciliation;
proven pre-mutation declines retain evidence without permanently preventing later explicit edits.

Opening an older snapshot of the same namespace preserves the live store handle and operational
history location. A conflicting saved location is refused before authored publication. Import
merges authored documents into the current native namespace. Saving a copy retains the original
history dependency; copy the referenced history directory as well when moving a workspace. The
normal Docker volume retains both. Authored backup recovery and project undo never roll back the
ledger. Native pointers are neither serialized nor restored, and saved receipts alone cannot
prove ownership in a new Live bridge epoch.

On open or applied backup recovery, POSIX native admission synchronizes the actual workspace
file and its containing directory, as well as the history location. The held regular-file descriptor
must retain the parsed native metadata and match the named inode before authorization. Failure
leaves authored state usable but native writes unavailable; a committed save with uncertain
durability cannot become authorized merely by reopening history in another directory.

The history store checks its held directory and writer-lock identities against the named paths
before and after reads and publication. Replacing either path revokes the stale handle's authority.
Windows can
inspect existing history but declines native dispatch until a supported directory-durability
primitive is available. The primary Docker server uses the POSIX implementation on every host.
