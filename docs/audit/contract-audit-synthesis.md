# Contract-Level Audit Synthesis

## Executive Summary

Eight parallel contract audits examined the Sunny theory engine and Score IR against
their formal specifications. The audits collectively identified **115 actionable
findings** across the full system surface.

| Severity | Count | Character |
|----------|-------|-----------|
| Critical | 29 | Undefined behaviour, data corruption, or invariant violation reachable from public API |
| High | 35 | Silent corruption propagating across subsystem boundaries without error signal |
| Medium | 30 | Localised degradation, heuristic fallback without diagnostic, or latent precondition gap |
| Low | 21 | Edge-case cosmetic errors, unreachable-in-practice paths, or precision concerns |

Two additional observations concern test coverage gaps rather than runtime defects.

The findings cluster around **six structural root causes** that each propagate across
multiple subsystems. Remediating these root causes rather than patching individual
findings reduces the 115 items to approximately 12 focused interventions.


## Audit Domains

| # | Domain | Scope | Agent | Findings |
|---|--------|-------|-------|----------|
| 1 | Primitive Type Alias Contracts | PTSP, PTMN, PTDI, PTPC + Score IR consumers | Type alias boundaries | 17 |
| 2 | Beat Rational Arithmetic Overflow | TNBT, RHEU, RHTS, RHTU, SITM | Integer overflow in exact arithmetic | 16 |
| 3 | Deserialisation Trust Boundary | SISZ, SIVD | JSON → struct invariant enforcement | 15 |
| 4 | Mutation and Undo Atomicity | SIMT | Transaction safety, closure identity capture | 16 |
| 5 | Temporal Pipeline Silent Drops | SITM, SICM, SIQR, SIHA | Error absorption in compilation | 13 |
| 6 | Voice Leading Array Bounds | VLNT, VLCN, VLFB, VLSC + SIMT | Cardinality validation, OOB access | 10 |
| 7 | Harmonic Analysis Degradation | HRFN, HRRN, HRCD, HRCS, HRST, SIHA | Silent heuristic fallback | 10 |
| 8 | Numerical Function Preconditions | TUET, TUJI, TUHT, ACHS, ACPL, ACRG, ACVP | Mathematical domain violations | 18 |


## Root Cause Analysis

The 115 findings reduce to six structural root causes. Each root cause is a single
design-level gap whose consequences surface in multiple audit domains.

### RC-A: Unvalidated Type Aliases (38 findings)

`PitchClass` (uint8_t, invariant [0,11]), `MidiNote` (uint8_t, invariant [0,127]),
`Interval` (int8_t), and `SpelledPitch.letter` (uint8_t, invariant [0,6]) are type
aliases that carry semantic invariants the type system cannot enforce. No construction
path validates these invariants structurally.

The `nat()` function at `PTSP001A.h:68` indexes a 7-element array by
`SpelledPitch.letter`; any value >= 7 is undefined behaviour. The pattern
`static_cast<PitchClass>(midi_value(sp) % 12)` appears in 6 Score IR files and
produces values in [244,255] for negative MIDI values, because C++ truncation-toward-zero
for negative operands yields negative remainders that wrap on unsigned cast.

**Affected audits**: 1 (all 17), 5 (#5, #8), 6 (#1-#3), 8 (#8).

**Remediation shape**: Strong types with validated construction (a `PitchClass` that
rejects values outside [0,11] at construction time), plus a `mod12_positive()` utility
that handles negative operands. Alternatively, a single `safe_pitch_class(int) -> PitchClass`
factory that normalises before storing.

### RC-B: Unbounded Integer Arithmetic in Beat (16 findings)

Beat uses `int64_t` for numerator and denominator. Every binary operator (`+`, `-`,
`*`, `/`, `<=>`) computes intermediate products that can overflow silently. The header
comment "Stored in lowest terms after every operation (prevents overflow)" is incorrect:
normalisation reduces stored values but cannot prevent intermediate product overflow.

The most dangerous path is `score_time_to_absolute_beat` (SITM001A.cpp:93), which
accumulates bar durations in a loop. A film score with 2000+ bars alternating between
coprime time signatures (7/8 and 5/16) overflows the denominator product chain,
corrupting every subsequent temporal coordinate.

**Affected audit**: 2 (all 16).

**Remediation shape**: Checked arithmetic (detect overflow before it occurs and return
an error), or `__int128` intermediates with post-operation range check, or a
safe-multiply-then-normalise sequence that reduces before the product exceeds 64 bits.

### RC-C: Deserialisation Trust Boundary (15 findings)

`score_from_json` calls `validate_structural` (rules S0-S11 only) rather than
`validate_score` (S + M + R rules), violating spec Section 13.3 which requires "full
validation runs before the document is made available." Five data fields
(`HarmonicAnnotation.chord`, `KeySignature.mode`, `Measure.local_time`,
`PartDefinition.range`, `TempoEvent` transition fields) are never serialised, so
round-trip permanently destroys them. Beat denominators of zero, enum values outside
defined ranges, duplicate IDs, and dangling references all pass through unchallenged.

**Affected audit**: 3 (all 15). **Feeds into**: RC-A (finding 1-4: deserialized
`SpelledPitch.letter` >= 7 triggers UB in `nat()`).

**Remediation shape**: Two tracks. First, serialise the five dropped fields (breaking
change to schema, requiring a version bump). Second, add post-parse validation for
Beat denominators > 0, enum range bounds, ID uniqueness, and referential integrity for
BeamGroup/Tuplet/NonChordTone references.

### RC-D: Non-Atomic Mutations and Stale Undo Closures (16 findings)

Multi-part mutations (`insert_measures`, `delete_measures`, `move_region`) modify
parts sequentially without rollback. If an intermediate step fails, the document is
left with parts at different measure counts and global maps shifted for bars that
don't exist in some parts. The undo mechanism compounds this: `undo()` pops the entry
before executing the inverse, so a failed inverse permanently loses the UndoEntry.

Undo closures capture `EventId` by value at mutation time. After a redo cycle,
`allocate_event_id()` assigns fresh IDs, making the closure's captured IDs stale.
Undo-after-redo silently fails to remove re-inserted events, causing unbounded document
growth on repeated undo-redo cycles. The `move_region` undo uses an ID-range filter
that can collaterally delete events created by unrelated mutations.

**Affected audit**: 4 (all 16).

**Remediation shape**: Three interventions. (1) Snapshot-based undo (capture document
state rather than closures) eliminates all stale-identity problems. (2) If closures
are retained, switch from EventId capture to content-hash or version-stamped identity.
(3) Multi-part mutations need either pre-validation that guarantees success or
copy-on-write rollback.

### RC-E: Silent Error Absorption in Compilation (13 findings)

`compile_to_midi` and `compile_to_note_events` use `continue` on failed temporal
conversions, silently dropping notes. `compute_tempo_beats` uses `value_or(Beat::zero())`
to relocate failed tempo events to the score origin. Structural validation rules S4-S6
check only the first entry of each global map, leaving subsequent entries with
`bar = 0` unconstrained. The return types carry no metadata about drop counts, so
callers have no completeness witness.

**Affected audit**: 5 (all 13).

**Remediation shape**: Replace `continue` with diagnostic accumulation (a
`CompilationReport` that lists every dropped event with reason). Extend S4-S6 to
validate all entries, not just the first. Add a `bar_number > 0` structural rule for
measures.

### RC-F: Missing Precondition Validation at API Boundaries (28 findings)

Voice leading functions accept mismatched cardinalities without validation
(`voice_lead_nearest_tone` silently extends targets by cycling). Tuning and acoustics
functions accept the full type range despite documented domain restrictions (`n > 0`,
`ratio > 0`, `freq > 0`). Six EDO functions crash on `n = 0` (integer division by
zero / SIGFPE). Harmonic analysis functions use independent root-finding algorithms
that can disagree, and the confidence field is set but never checked by any downstream
consumer.

**Affected audits**: 6 (10), 7 (10), 8 (18). Overlap where multiple audits found the
same root cause in different subsystems.

**Remediation shape**: Add precondition guards returning `std::expected` (or
`std::optional`) for functions that currently accept invalid inputs silently. For
voice leading, validate cardinality equality before dispatch. For tuning/acoustics,
guard `n > 0` / `ratio > 0` at each function boundary.


## Cross-Domain Dependency Graph

Several findings create cascading failure chains where a defect in one subsystem
amplifies through others.

```
RC-C (Deserialisation)
  │
  ├─→ RC-A (Type Aliases)
  │     SpelledPitch.letter >= 7 from JSON triggers UB in nat()
  │     Propagates through: midi_value → compile_to_midi → MIDI output
  │                         line_of_fifths → default_spelling → Score IR views
  │                         to_spn → serialisation → round-trip corruption
  │
  ├─→ RC-B (Beat Overflow)
  │     Beat{1, 0} from JSON triggers division-by-zero in all arithmetic
  │     Propagates through: score_time_to_absolute_beat → all temporal queries
  │
  ├─→ RC-D (Mutation Atomicity)
  │     Deserialized duplicate EventIds cause undo closures to target wrong events
  │
  └─→ RC-E (Silent Drops)
        Deserialized bar_number = 0 triggers silent note drops in compilation

RC-A (Type Aliases)
  │
  ├─→ RC-E (Silent Drops)
  │     midi_value() < 0 or > 127 triggers `continue` in compile_to_midi
  │
  └─→ RC-F (Preconditions)
        PitchClass > 11 passed to negative_harmony, chord-scale analysis
        MidiNote > 127 passed to voice_lead_nearest_tone
```

The deserialisation trust boundary (RC-C) is the widest entry point: a single
malformed JSON document can trigger defects from four other root causes simultaneously.


## Unified Critical Findings

The following 29 critical findings are ordered by blast radius (number of downstream
consumers that observe corrupted state from a single triggering input).

### Tier 1: System-Wide Corruption (blast radius = entire score)

| ID | Root Cause | Source | Description |
|----|-----------|--------|-------------|
| 2.1 | RC-B | SITM001A.cpp:93 | Cumulative `operator+` in `score_time_to_absolute_beat` overflows for coprime time signatures across long scores. Corrupts every temporal coordinate. |
| 2.2 | RC-B | TNBT001A.h:151 | `operator+`/`operator-` denominator product `a.den * b.den` overflows for coprime denominators from nested tuplets. |
| 2.4 | RC-B | TNBT001A.h:170 | `operator*` numerator/denominator products overflow for values > sqrt(INT64_MAX). |
| 5.6 | RC-E | SITM001A.cpp:63 | `compute_tempo_beats` uses `value_or(Beat::zero())`, relocating failed tempo events to score origin. Corrupts all real-time values. |
| 3.1 | RC-C | SISZ001A.cpp:963 | `score_from_json` calls `validate_structural` only, not `validate_score`. Musical and rendering rules never run on load. |

### Tier 2: Subsystem-Wide Corruption (blast radius = one part or all parts)

| ID | Root Cause | Source | Description |
|----|-----------|--------|-------------|
| 1.1 | RC-A | PTSP001A.h:68 | `nat(letter >= 7)` reads past 7-element array. UB from OOB read. |
| 1.2 | RC-A | PTSP001A.cpp:18 | `to_spn(letter >= 7)` reads past 7-element array. UB. |
| 1.3 | RC-A | PTSP001A.h:128 | `line_of_fifths_position(letter >= 7)` reads past 7-element array. UB. |
| 1.4 | RC-C+A | SISZ001A.cpp:30 | `spelled_pitch_from_json` accepts `letter: 200` without validation. Propagates UB to all SpelledPitch consumers. |
| 1.5 | RC-A | PTSP001A.cpp:54 | `from_spn` int8_t accidental overflow on 128+ sharps. Signed integer overflow is UB. |
| 1.6 | RC-A | HRCS001A.cpp:43 | `analyze_chord_scale` with negative Interval: `(root + interval) % 12` yields negative PitchClass, OOB array read. |
| 3.2 | RC-C | SISZ001A.cpp:543 | `HarmonicAnnotation.chord` never serialised. Round-trip destroys all ChordVoicing data irreversibly. |
| 3.3 | RC-C | SISZ001A.cpp:42 | `beat_from_json` constructs `Beat{1, 0}` without denominator check. Division-by-zero in all subsequent arithmetic. |
| 4.1 | RC-D | SIMT001A.cpp:644 | `insert_measures` modifies parts sequentially with no rollback. Partial failure leaves parts at different measure counts. |
| 4.2 | RC-D | SIMT001A.cpp:740 | `delete_measures` same non-atomic pattern as 4.1. |
| 4.3 | RC-D | SIMT001A.cpp:1606 | `move_region` undo uses ID-range filter that can delete events from unrelated mutations. |
| 5.1 | RC-E | SICM001A.cpp:285 | `compile_to_midi` silently drops all notes in a measure with `bar_number = 0`. |
| 5.2 | RC-E | SICM001A.cpp:161 | `compile_to_midi` silently drops tempo events at bar 0. All subsequent timing wrong. |
| 5.3 | RC-E | SICM001A.cpp:174 | `compile_to_midi` silently drops time signature events at bar 0. |
| 6.1 | RC-F | VLNT001A.cpp:62 | `voice_lead_nearest_tone` silently extends targets by cycling when cardinalities mismatch. Bijection invariant violated. |
| 6.2 | RC-F | VLNT001A.cpp:389 | `voice_lead_optimal` same silent extension. Hungarian algorithm receives degenerate matrix. |
| 6.3 | RC-F | SIMT001A.cpp:2290 | `apply_voice_leading` passes mismatched cardinalities to `voice_lead_nearest_tone`. Document partially mutated. |
| 6.10 | RC-F | VLFB001A.cpp:143 | `diatonic_above` with `generic_interval=0`: `(-1) % scale_size` can be -1 in C++, indexing `scale[-1]`. UB. |
| 8.1 | RC-F | TUET001A.h:33 | `edo_step_cents(0)`: `1200.0 / 0` = +inf. Propagates through 6 downstream functions. |
| 8.2 | RC-F | TUET001A.h:56 | `edo_frequency(_, 0)`: `pow(2.0, x/0)` = NaN. All acoustics consumers receive NaN. |
| 8.3 | RC-F | TUET001A.h:68 | `ratio_to_cents(0.0)`: `log2(0)` = -inf. Propagates through 5 functions, 3 subsystems. |
| 8.8 | RC-F | TUHT001A.h:155 | `tempered_frequency(-1, ...)`: array index -1. UB from OOB read. |
| 8.9 | RC-F | TUET001A.h:94 | `edo_transpose(_, _, 0)`: integer modulo zero. SIGFPE crash. |
| 8.10 | RC-F | TUET001A.h:108 | `edo_invert(_, _, 0)`: integer modulo zero. SIGFPE crash. |
| 8.11 | RC-F | TUET001A.h:122 | `edo_interval_class(_, _, 0)`: integer modulo zero. SIGFPE crash. |


## Remediation Priority

The six root causes form a natural dependency order for remediation. Fixing
deserialisation first prevents external data from triggering defects in the other
five domains. Fixing type aliases second eliminates the largest single class of UB.

### Phase 1: Trust Boundaries (RC-C, then RC-A)

**RC-C first** because the deserialisation path is the widest entry point for
external data. Serialise the five dropped fields, add post-parse invariant checks
(Beat denominator > 0, SpelledPitch.letter < 7, enum bounds, ID uniqueness,
referential integrity), and upgrade to `validate_score` on load.

**RC-A second** because it eliminates UB from internal arithmetic. Introduce
validated construction for PitchClass, MidiNote, and SpelledPitch.letter. Add
`mod12_positive()` to replace all `midi_value() % 12` patterns. Guard
`apply_interval` against int8_t overflow.

### Phase 2: Arithmetic Safety (RC-B, then RC-F)

**RC-B** because Beat overflow corrupts all temporal coordinates downstream. Add
checked multiplication with overflow detection to all Beat operators. The header
comment at TNBT001A.h:14 must be corrected.

**RC-F** because unguarded preconditions produce SIGFPE crashes and silent NaN/inf
propagation. Guard each tuning/acoustics function with its documented domain
restriction. Guard voice leading with cardinality checks. Guard harmonic analysis
with consistent root-finding.

### Phase 3: Transaction Safety and Error Reporting (RC-D, RC-E)

**RC-D** because mutation atomicity requires architectural change (snapshot undo or
copy-on-write rollback). The stale-identity problem in undo closures is the most
complex remediation.

**RC-E** because replacing `continue` with diagnostic accumulation is
straightforward once the compilation pipeline is stable. Extend S4-S6 validation to
cover all map entries.


## Test Coverage Gaps

Both the deserialisation test suite (TSSI004A) and the mutation test suite (TSSI007A)
lack negative-path coverage. No test constructs invalid input to verify rejection.
No test injects partial failure to verify atomicity. No test exercises undo-redo-undo
cycles. These gaps mean all 115 findings are latent: they exist in the code but have
no regression test that would detect their introduction or verify their remediation.

Remediation for each root cause should be paired with targeted negative-path tests
that exercise the exact trigger conditions catalogued in this audit.


## Appendix: Per-Domain Findings Summary

### A1. Primitive Type Alias Contracts (17 findings)

5 Critical, 7 High, 2 Medium, 3 Low.

Root causes: `SpelledPitch.letter` indexes 7-element arrays without validation
(findings 1-4). `from_spn` accumulates int8_t accidentals without overflow guard
(finding 5). `midi_value() % 12` produces negative PitchClass on 6 Score IR sites
(findings 9-15). `apply_interval` truncates int8_t octave/accidental (findings 6-7).
`closest_pitch_class_midi` returns MidiNote > 127 for PitchClass > 11 (finding 13).

### A2. Beat Rational Arithmetic Overflow (16 findings)

4 Critical, 3 High, 5 Medium, 4 Low.

Root causes: `operator+`/`operator-` denominator products overflow for coprime
time signatures (findings 1-3). `operator*` products overflow for values >
sqrt(INT64_MAX) (finding 4). Comparison operator cross-multiplication overflows,
corrupting ordering/sorting (finding 8). `score_time_to_absolute_beat` loop
accumulates overflow over many bars (finding 1). `PositiveRational` comparisons
share the same cross-product overflow (finding 13).

### A3. Deserialisation Trust Boundary (15 findings)

3 Critical, 4 High, 4 Medium, 3 Low, 1 Observation.

Root causes: `validate_structural` called instead of `validate_score` (finding 1).
Five fields never serialised: `HarmonicAnnotation.chord` (finding 2),
`TempoEvent` transition fields (finding 4), `KeySignature.mode` (finding 5),
`Measure.local_time` (finding 6), `PartDefinition.range`/`articulation`/`rendering`
(finding 7). `Beat{_, 0}` accepted without denominator check (finding 3). No ID
uniqueness validation (finding 9). No referential integrity for BeamGroup, Tuplet,
or NonChordTone references (findings 10-12). Enum ranges unchecked (finding 14).

### A4. Mutation and Undo Atomicity (16 findings)

2 Critical, 7 High, 5 Medium, 2 Low, 1 Observation.

Root causes: Multi-part mutations lack rollback (`insert_measures` finding 1,
`delete_measures` finding 2). Undo closures capture stale EventId after redo
(`delete_event` finding 5, `insert_note` finding 6, `copy_region`/`reorchestrate`/
`double_at_interval` finding 7). `move_region` undo uses fragile ID-range filter
(finding 3/8). Group undo/redo short-circuits on partial failure (findings 10-11).
`undo()` pops entry before execution, losing it on failure (finding 16).
`reorder_parts` accepts duplicate PartIds, destroying parts (finding 4).
`next_event_id` global counter has no synchronisation (finding 9).

### A5. Temporal Pipeline Silent Drops (13 findings)

4 Critical, 5 High, 2 Medium, 1 Low, 1 High (meta).

Root causes: `compile_to_midi` uses `continue` on failed temporal conversions
(findings 1, 4). Global map validation (S4-S6) checks only first entry (findings
2-3). `compute_tempo_beats` uses `value_or(Beat::zero())` relocating failed events
(finding 6). `compile_to_note_events` drops events with same patterns (findings 7-8).
Return types carry no drop-count metadata (finding 10). `derive_for_range` sets
confidence without downstream gating (finding 12).

### A6. Voice Leading Array Bounds (10 actionable findings)

4 Critical, 2 High, 4 Moderate.

Root causes: `voice_lead_nearest_tone` and `voice_lead_optimal` extend targets
by cycling when cardinalities mismatch (findings 1-2). Score IR `apply_voice_leading`
passes mismatched cardinalities (finding 3). `diatonic_above` indexes array at -1
(finding 10). `check_voice_leading` silently ignores extra voices via `std::min`
(finding 8). `realise_figured_bass_sequence` silently drops voice-leading continuity
at cardinality transitions (finding 9). Voicing generators return identity on
insufficient input without error (findings 4-7). Species counterpoint (VLSC001A)
correctly validates all five species.

### A7. Harmonic Analysis Degradation (10 findings)

0 Critical, 2 High, 5 Medium, 3 Low.

Root causes: `recognize_chord` and `analyze_chord_function` use independent
root-finding algorithms that can disagree (finding 1). Confidence field is dead
metadata (finding 2). `chord_to_numeral` discards `is_minor` parameter (finding 3).
Flat-first heuristic miscategorises #IV as bV (finding 4). `find_degree` snaps
chromatic roots to nearest diatonic degree (finding 5). `set_section_harmony` string
matcher misses suffixed numerals and iii/III (findings 6, 10). Dim7 symmetry resolved
by lowest PitchClass rather than context (finding 9).

### A8. Numerical Function Preconditions (18 actionable findings)

7 Critical, 5 High, 3 Medium, 3 Low/Negligible.

Root causes: `edo_step_cents(0)` divides by zero producing +inf (finding 1).
`edo_transpose`/`invert`/`interval_class` with n=0 cause SIGFPE (findings 9-11).
`tempered_frequency` with negative pitch_class indexes array at -1 (finding 8).
`ratio_to_cents(0)` produces -inf (finding 3). `JIRatio{0,0}.to_double()` produces
NaN (finding 5). `ji_multiply` overflows int64_t (finding 6). `edo_approximate_ratio(0, _)`
triggers UB via `round(-inf)` (finding 19). No function in tuning or acoustics
validates its documented preconditions.
