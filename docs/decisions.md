# Sunny — Decision Record

This file is the durable record of engineering decisions: what was
decided, on what evidence, and what was deliberately accepted rather
than fixed. Entries are append-mostly; a reversed decision gets a new
entry pointing at the one it supersedes. The audit history it preserves
was previously deleted from the tree (`b6ce9b7 "Remove."`), which cost
the project its record of fifteen accepted-as-limitation findings; this
file exists so that does not happen again.

## 1. Audit history (2026-02 to 2026-03)

A contract-level audit (committed `da30a29`, 115 findings) attributed
the defect population to six root causes. Four remediation waves
followed. The full synthesis is preserved in
[audit/contract-audit-synthesis.md](audit/contract-audit-synthesis.md);
the causes, their attributed design decisions, and their outcomes:

| Root cause | Design gap | Fix taken | Outcome |
|---|---|---|---|
| RC-A Unvalidated type aliases | `PitchClass`/`MidiNote` as bare `uint8_t` aliases; no validated construction | Wave 1: helpers + asserts (declined the recommended strong types) | Recurred in waves 2 and 3; strong types adopted in the 2026-07 programme |
| RC-B Beat arithmetic overflow | `int64` num/den with a false "lowest terms prevents overflow" assumption | `__int128` intermediates, checked ops, `ArithmeticOverflow` | Held; only adjacent sites needed later patches |
| RC-C Deserialisation trust | Load path ran structural rules only; five fields never serialised | Validation-rule framework (S12–S16), schema v3, typed checks | Held; extended rather than re-fixed in later waves |
| RC-D Undo atomicity | Closure-captured inverses; stale `EventId`s; pop-before-execute | Waves 1–4: local patches (declined the recommended snapshot undo) | Recurred in every wave; snapshot engine adopted in the 2026-07 programme |
| RC-E Silent compilation drops | `continue` on failed conversions; no completeness witness | `CompilationReport` diagnostic-carrying returns | Held; a truncation variant (uint8 loop counters) fixed in wave 4 |
| RC-F Missing preconditions | Full-range parameters despite documented domain restrictions | `Result<T>` boundaries, domain error codes | Held; rolled out per-subsystem |

The pattern is the programme's central lesson: **the two root causes
fixed with the recommended structural change never recurred; the two
fixed with local patches recurred in every subsequent wave.**

Wave 3 (`701c7ae`) closed 15 findings as "accepted as design
limitations with documented rationale", but the rationale was never
committed; those decisions are unrecoverable and stand only as the
commit-message summary.

## 2. Decisions of the 2026-07 structural remediation

**D1 — Single MCP authority: the C++ `sunny-mcp` binary.** The Python
FastMCP server was deleted rather than repaired: it crashed at startup
(nonexistent attribute in its lifespan), spoke OSC/UDP 11000/11001 to a
Remote Script listening on TCP 9001 with length-prefixed JSON, and its
engine wrapper passed strings where the binding takes interval lists.
The surviving Python surface is a thin scripting facade (`sunny.core`)
plus `SunnyRemoteScript`.

**D2 — Transport survivor: `Bridge/INTP001A`, not `Transport/INTP001A`.**
The original plan designated the Transport-directory implementation as
survivor; execution-time evidence reversed this. The Bridge transport
is newer (same commit as the Remote Script), request/response-shaped
like the wire protocol, and consumed by every FMAL compiler; the
Transport pair had no tests, no non-test consumers, and a factory that
returned the wrong type. One transport, one seam (`LomTransport`),
port 9001.

**D3 — WPOSC001A and RTLF001A stay.** The OSC codec and the SPSC ring
buffer looked orphaned but form the reserved Max-for-Live UDP path, and
the ring buffer has a production consumer (`sunny.parameter~`). No UDP
transport ships until that path has a consumer; the in-device default
port for it is 9877.

**D4 — Configuration is environment-only.** `sunny.toml.example`
documented a file no code ever parsed; deleted. The binary reads
exactly `SUNNY_ABLETON_HOST` and `SUNNY_TCP_PORT`, documented in
`.env.example`. Offline (unset host) is a supported mode in which
Ableton-mutating tools decline loudly.

**D5 — One error taxonomy.** The four per-domain `constexpr int`
namespaces were folded into the `ErrorCode` enum with their numeric
values preserved (ranges: 5xxx Score, 6xxx Timbre, 7xxx Mix, 8xxx
Corpus). `Diagnostic.error_code` is typed `ErrorCode`; ints appear only
at JSON boundaries.

**D6 — One temporal serialisation scheme.** `SISZ002A` owns
Beat/ScoreTime JSON (nested `{num, den}`, refusal on missing fields and
non-positive denominators). Corpus moved to schema v2 (nested,
refuse-on-missing for unconditionally-written fields, validate-on-load)
with a permanent v1 lenient reader; three v1 fixtures are frozen in
TSCI009A and must keep loading. SpelledPitch keys still differ between
Score ("accidental"/"octave") and Timbre ("acc"/"oct"); accepted — not
worth a schema break in both families. Load blocked by validation
returns `ValidationOnLoadFailed` uniformly.

**D7 — Snapshot undo.** The closure-based undo engine (RC-D) was
replaced by pre-mutation document snapshots: undo/redo swap the live
document with a snapshot, groups collapse to one snapshot, and
mid-mutation failures restore the snapshot, making multi-part mutations
atomic by construction. Known behavioural delta, judged a defect fix:
the closure engine left `stale_harmonic_regions` residue after undo,
violating the component's stated invariant ("mutation + inverse
restores previous state exactly"); the snapshot engine restores the
whole document. TSSI014A pins whole-document identity.

**D8 — Freestanding Max layer.** `Sunny.Max` externals cannot link the
full engine; `VoiceLeadStandalone.h` deliberately re-implements
nearest-tone voice leading (see VLNT001A for the engine version). This
duplication is a recorded decision, not drift.

**D9 — Spec-mandated, test-only theory domains stay.** Acoustics,
Tuning, Transform, and PostTonal have no production consumers yet; they
implement formal-spec chapters (§11–§14) and remain as library surface.
Exposing them as MCP tools is future feature work, not a defect.

**D10 — CI gates that can fail.** `python-test` and mypy failures now
fail the build; the empty-artifact path that made them permanently
green is fixed. Bandit stays advisory (`continue-on-error`): its one
recurring finding is the Remote Script's deliberate `0.0.0.0` bind,
required for WSL2-to-Windows cross-host connection.

**D11 — Uninitialised POD members get default initialisers.**
`ScaleDefinition.intervals/note_count`, `KeySignature.accidentals`, and
`SpelledPitch` members serialised indeterminate bytes when
default-initialised (surfaced by the undo identity tests). Value types
that can reach a serialiser must be fully default-initialised.

## 3. Known follow-ups (deferred, with acceptance criteria)

- **Live Ableton validation.** The loopback tests pin both ends of the
  wire protocol; an end-to-end run against a real Ableton session
  (WSL2 → Windows host, Remote Script installed) is the remaining
  proof. Acceptance: `create_progression_clip` produces a playable clip
  in a live set; declines are clean when Ableton quits mid-session.
- **Strong types beyond PitchClass/MidiNote.** `Interval`, `Velocity`,
  and `SpelledPitch.letter` remain aliases with helper-based guards.
  Adopt the same validated-value-type pattern if their invariants are
  violated again.
- **Orchestrator undo model.** `INOR001A` still uses closure-based
  operations (its undo emits compensating Ableton commands, a different
  semantic from document restore). Revisit when the live path is
  validated.
- **CMake globs.** Source lists use `file(GLOB_RECURSE)` without
  `CONFIGURE_DEPENDS`; new files need an explicit reconfigure. Accepted
  for now (presets make reconfigure cheap); revisit if it bites again.
- **MCP server test double.** TSMC001A re-implements the JSON-RPC
  dispatch logic instead of exercising `McpServer::handle_request`;
  the duplicated dispatch can drift from the real one. Expose a test
  seam instead.
