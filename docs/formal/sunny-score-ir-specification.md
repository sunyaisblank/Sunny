# Sunny Score IR — Formal Specification

**Version:** 0.2.0-draft
**Date:** 2026-02-12
**Status:** Normative model with an explicit implemented runtime profile
**Dependency:** Sunny Engine Formal Specification v0.1.0 (the "Theory Spec")

---

## 0. Preamble

### 0.1 Purpose

This document defines the formal specification for the Sunny Score IR (Intermediate Representation), a hierarchical document model for representing the symbolic, temporal, expressive, and tuning content of musical works as structured, queryable, manipulable objects. Score is authoritative for that domain. A complete production target is governed by the cross-IR Project tuple (Score, Timbre, Mix); Score alone does not claim device-chain, synthesis, routing, gain-structure, or audible-output authority.

The Score IR occupies a distinct architectural layer from the Sunny music theory engine. The theory engine provides the vocabulary: pitch classes, intervals, chords, scales, voice-leading constraints, formal templates. The Score IR provides the document: the concrete instantiation of that vocabulary into a particular work, with particular instruments, particular notes at particular times, particular dynamics and articulations, arranged into a particular formal structure. The relationship is analogous to that between a programming language specification and a source file written in that language.

### 0.2 Design Objectives

The Score IR is designed to satisfy five constraints simultaneously:

1. **Domain completeness**: Every symbolic, temporal, expressive, articulation-mapping, and tuning property owned by Score must be representable. A Score contains sufficient information for conventional notation and nominal performance-event compilation without hidden musical context. Timbre/device realization and Mix/routing realization are deliberately separate Project inputs, and every target capability gap remains an explicit compilation residual.

2. **Semantic fidelity**: The representation preserves compositional intent, not merely acoustic outcome. A staccato quarter note and a short eighth note followed by an eighth rest may produce identical MIDI output, but they carry different meanings and must remain distinguishable in the IR.

3. **Structural navigability**: An agent must be able to query the document at any level of the hierarchy — "what is the harmony at bar 47?", "which instruments carry the melody in the development section?", "show me all instances of the opening motif" — without scanning the entire document linearly.

4. **Incremental mutability**: An agent must be able to modify the document at any granularity — insert a note, reorchestrate a passage, restructure a section — while the IR maintains its internal consistency invariants automatically or reports violations explicitly.

5. **Deterministic compilation**: The mapping from Score IR to each rendering target is reproducible. File targets are total for valid input. A stateful DAW target additionally returns capability diagnostics when its public API cannot represent an IR feature; it must never guess a substitute operation.

### 0.3 Notation Conventions

This specification inherits the notation conventions of the Theory Spec (§0.3). Additional conventions:

- Hierarchical paths are written with dot notation: `score.parts[3].measures[47].voices[0].events[12]`.
- Time positions within the score are written as **ScoreTime**(*bar*, *beat*), where *bar* is a 1-indexed measure number and *beat* is a Beat value (exact rational) within the measure.
- Durations are always **Beat** values (exact rationals) from the Theory Spec.
- Optional fields are written as `Option<T>`, indicating the value may be absent.
- Ordered collections are written as `Vec<T>`; associative mappings as `Map<K, V>`.
- Unique identifiers are written as `Id<T>`, opaque handles that support equality comparison and hashing.

### 0.4 Relationship to Theory Spec Types

The Score IR imports the following types from the Theory Spec by reference:

| Type | Theory Spec Section | Usage in Score IR |
|------|-------------------|-------------------|
| PitchClass | §2.4, §15.1.1 | Harmonic analysis queries |
| SpelledPitch | §2.5, §15.1.2 | Note events |
| Interval | §3.2, §15.1.3 | Melodic/harmonic analysis |
| Beat | §9.1, §15.1.4 | All durations and time positions |
| ChordVoicing | §15.1.6 | Harmonic annotations |
| ScaleDefinition | §15.1.7 | Key context |
| TimeSignature | §15.1.8 | Metrical structure |
| HarmonicState | §15.1.11 | Harmonic analysis layer |

The Score IR does not redefine these types. Where the Score IR requires properties not present in the Theory Spec types, it wraps them in Score IR–specific structures that compose with, rather than duplicate, the theory layer.

### 0.5 Implemented Runtime Profile

The current Sunny runtime implements the C++ document model, validation and workflows; versioned JSON serialisation; MIDI event compilation; MusicXML and LilyPond text compilation; and capability-aware Ableton deployment. The public MCP surface is the inventory in §12.2; `tools/list` is the runtime authority. There is no binary Score IR codec and no in-process PCM `AudioCompiler`; audio is produced by the deployed Ableton session after Score, Timbre, and Mix compilation. Sections that discuss either binary encoding or direct audio rendering define reserved design space, not a callable runtime capability.

---

## 1. Document Hierarchy

### 1.1 Structural Overview

The Score IR is a tree with five principal levels. Each level owns its children and provides the context within which its children are interpreted.

```
Score
 ├── ScoreMetadata
 ├── ScoreTuning
 ├── TempoMap
 ├── GlobalKeySignatureMap
 ├── GlobalTimeSignatureMap
 ├── SectionMap
 ├── RehearsalMarkMap
 └── Part[]
      ├── PartDefinition
      └── Measure[]
           ├── MeasureContext (inherited or overridden)
           └── Voice[]
                └── Event[]
                     ├── Note
                     ├── Rest
                     ├── ChordSymbol
                     └── Direction
```

The tree is not balanced: different parts may have different numbers of voices in a given measure, and the event density varies freely. The global maps (tempo, key signature, time signature, sections) span the entire score and apply to all parts unless locally overridden.

### 1.2 Addressing

Every node in the hierarchy is addressable by a structural path. Two addressing schemes are supported:

**Hierarchical address**: A sequence of indices descending the tree. Example: `Part(2).Measure(47).Voice(0).Event(12)` identifies the 13th event in the first voice of the 48th measure of the third part.

**Temporal address**: A (part, time) pair where time is a ScoreTime value. Example: `Part(2).At(bar=47, beat=Beat(3,4))` identifies whatever event is sounding in part 2 at the third beat of bar 47. This resolves to a hierarchical address by searching the measure and voice structure.

**Region address**: A bounded span of score time, optionally restricted to a subset of parts. Example: `Region(parts=[0,1,2], from=ScoreTime(33,Beat(0,1)), to=ScoreTime(64,Beat(0,1)))` selects all content in parts 0–2 from the start of bar 33 to the start of bar 64. Regions are the primary operand for bulk operations (reorchestration, transposition, dynamic scaling).

---

## 2. Score Level

### 2.1 Score

**Definition 2.1.1**. A *Score* is the root node of the document hierarchy.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<Score>` | Positive repository-selected document/lineage identifier; zero is the unassigned sentinel |
| `metadata` | `ScoreMetadata` | Descriptive metadata |
| `tuning` | `ScoreTuning` | Complete sounding-pitch function over the Score note-index domain |
| `tempo_map` | `TempoMap` | Tempo as a function of score time |
| `key_map` | `KeySignatureMap` | Key signatures as a function of score time |
| `time_map` | `TimeSignatureMap` | Time signatures as a function of score time |
| `section_map` | `SectionMap` | Formal structure annotations |
| `rehearsal_marks` | `Vec<RehearsalMark>` | Named navigation points |
| `parts` | `Vec<Part>` | Ordered list of instrumental parts |
| `harmonic_annotations` | `HarmonicAnnotationLayer` | Global harmonic analysis (§6) |
| `orchestration_annotations` | `OrchestrationLayer` | Textural role assignments (§7) |
| `tone_row` | `Option<ToneRow>` | Optional governing twelve-tone row (§11) |
| `stale_harmonic_regions` | `Vec<ScoreRegion>` | Harmonic-analysis regions invalidated by edits (§7.2) |
| `stale_orchestration_regions` | `Vec<ScoreRegion>` | Orchestration-analysis regions invalidated by edits (§7.2) |
| `version` | `u64` | Monotonically increasing edit counter |
| `identity_reservations` | `ScoreIdentityReservations` | Persistent typed sets of actually exposed Event, Part, Section, Tuplet, and BeamGroup IDs, including retired identities |

Root identity belongs to the repository that owns multiple documents, not to a pure transformation
or a process-global allocator. `ScoreSpec` carries the caller-selected identity (standalone creation
defaults to one). Every API that constructs a derived standalone Score requires an explicit result
identity and starts that new lineage at version one. In the MCP repository, the public `score_id`
handle and stored `Score.id` are the same value; `score_get_json` therefore cannot expose a second,
contradictory root identity. Repository allocation admits `UINT64_MAX` once, then reports exhaustion
without wrapping or overwriting an existing document.

**Invariants**:
- `parts` is non-empty.
- All parts have the same total duration (measured in score time); if a part has fewer notated events, it is padded with implicit rests.
- `tempo_map`, `key_map`, and `time_map` are defined for the entire duration of the score (from bar 1 through the final bar).

#### 2.1.1 ScoreTuning

`ScoreTuning` makes the pitch semantics of every performance-admitted Score note explicit rather
than relying on an ambient synthesizer or DAW setting. Its tractable domain is the closed MIDI/Live
note-index interval `[0,127]`. A structurally retained `SpelledPitch` outside that interval remains
an explicit compiler drop; it does not acquire an invented wrapped, clamped, or periodic frequency.
Its fields are:

| Field | Type | Description |
|---|---|---|
| `name` | `String` | Descriptive source label; it does not determine pitch |
| `reference_midi_note` | `u8` | Reference index in `[0,127]` |
| `reference_frequency_hz` | `f64` | Finite positive frequency assigned exactly to the reference index |
| `cents_from_reference` | `[f64; 128]` | One relative cent position for every Score/MIDI note index |

For note index *m*, the sounding-frequency intent is

`frequency(m) = reference_frequency_hz × 2^(cents_from_reference[m] / 1200)`.

The reference entry is exactly zero. Every table entry and every frequency derived by this
equation must be finite and positive in the implementation's `double` domain. The representation
does not impose monotonicity, twelve-degree cardinality, or octave recurrence: those are properties
of particular tunings, not of the complete finite pitch function. The canonical default is
12-TET over indices 0–127 with MIDI note 69 at 440 Hz. `SpelledPitch` still owns notation spelling;
its MIDI index selects the corresponding entry in this independent sounding-pitch function.

### 2.2 ScoreMetadata

**Definition 2.2.1**. Descriptive metadata for the score.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `title` | `String` | Work title |
| `subtitle` | `Option<String>` | Movement title or subtitle |
| `composer` | `Option<String>` | Composer attribution |
| `arranger` | `Option<String>` | Arranger attribution |
| `opus` | `Option<String>` | Opus or catalogue number |
| `created_at` | `Timestamp` | Document creation time |
| `modified_at` | `Timestamp` | Last modification time |
| `total_bars` | `u32` | Total number of measures |
| `tags` | `Vec<String>` | Genre, style, or project tags |

### 2.3 TempoMap

**Definition 2.3.1**. The *TempoMap* is a piecewise function from score time to tempo, supporting instantaneous changes, gradual transitions, and metric modulations.

**Representation**: An ordered sequence of *TempoEvent* entries:

| Field | Type | Description |
|-------|------|-------------|
| `position` | `ScoreTime` | Where this tempo event takes effect |
| `bpm` | `PositiveRational` | Tempo in beats per minute (exact rational; distinct from Beat, which represents duration) |
| `beat_unit` | `BeatUnit` | Which note value receives the beat (quarter, dotted quarter, half, etc.) |
| `transition` | `TempoTransition` | How the previous tempo changes to this one |

*Note*: `PositiveRational` is a canonical strictly positive rational number (p/q where p, q ∈ ℤ⁺ and gcd(p,q) = 1). It carries rate semantics rather than Beat's duration semantics, preventing accidental rate-duration composition. Every C++ value normalises at construction, exposes read-only components, and uses `PositiveRational::from_ratio(p,q)` for checked dynamic input. Score JSON readers reject non-positive components and writers emit the stored lowest-term pair. Thus equivalent spellings such as 240/2 and 120/1 have one source identity before tempo resolution, project planning, or target projection.

**TempoTransition** variants:

| Variant | Semantics |
|---------|-----------|
| `Immediate` | Instantaneous tempo change |
| `Linear(duration: Beat)` | Incoming linear interpolation from the previous event's effective quarter tempo to this event's effective quarter tempo |
| `MetricModulation { old_unit: BeatUnit, new_unit: BeatUnit }` | Instantaneous equality of the previous subdivision and the new subdivision; the stored target BPM is checked against the exact ratio |

**BeatUnit** enumeration: `Whole`, `Half`, `DottedHalf`, `Quarter`, `DottedQuarter`, `Eighth`, `DottedEighth`, `Sixteenth`.

The BeatUnit determines the denominator of the BPM fraction. A tempo of "dotted quarter = 120" in 6/8 means 120 dotted-quarter notes per minute; the effective eighth-note rate is 360 per minute.

**Resolution**: Let *Qᵢ* be event *i*'s exact effective quarter-note BPM and *xᵢ* its
AbsoluteBeat. At an event point, that event's target rate is active. In an open interval
`[xᵢ, xᵢ₊₁)`, the rate is constant at *Qᵢ* unless event *i+1* is `Linear`; in that case:

`Q(x) = Qᵢ + ((x - xᵢ) / (xᵢ₊₁ - xᵢ)) · (Qᵢ₊₁ - Qᵢ)`.

Thus `transition` is owned by its destination, not its source. A Linear `duration` must equal the
entire exact interval `xᵢ₊₁ - xᵢ`; shorter values that would imply an undocumented hold or jump are
invalid. For a MetricModulation, `beat_unit = new_unit` and
`Qᵢ₊₁ = Qᵢ · duration(new_unit) / duration(old_unit)` exactly.

**Invariant**: The TempoMap begins exactly at `ScoreTime(1, Beat(0,1))`; its first event is
`Immediate`; each BPM is a construction-safe canonical `PositiveRational`; transition and BeatUnit
enum payloads are in-domain; only Linear carries a positive `linear_duration`; and S24 proves every
Linear or MetricModulation relationship before clock conversion or target projection. Positivity
and lowest-term rate identity are type/parser invariants rather than states discovered by S24.

### 2.4 KeySignatureMap

**Definition 2.4.1**. The *KeySignatureMap* is a piecewise-constant function from score time to key signature.

**Representation**: An ordered sequence of entries:

| Field | Type | Description |
|-------|------|-------------|
| `position` | `ScoreTime` | Bar and beat where the key signature takes effect |
| `key` | `KeySignature` | The key signature |

**KeySignature**:

| Field | Type | Description |
|-------|------|-------------|
| `root` | `SpelledPitch` | Tonic (letter + accidental; octave ignored) |
| `mode` | `ScaleDefinition` | Scale type (major, minor, dorian, etc.) |
| `accidentals` | `i8` | Signed count: positive for sharps, negative for flats |

**Derivation and identity**: For major, minor, Ionian, Dorian, Phrygian, Lydian,
Mixolydian, Aeolian, and Locrian, `accidentals` is derivable from the tonic letter,
tonic accidental, and mode. S21 requires the stored value to equal that derivation. For an
unrecognized/custom or registered non-diatonic `ScaleDefinition`, the relation is not statically
known and the explicitly stored `accidentals` remains authoritative. `ScaleDefinition.name` and
`description` are owned strings, not borrowed views. A compilable definition has 1–12 active
pitch-class offsets, begins at 0, increases strictly inside `[0,11]`, and zeroes its unused fixed
capacity. If its non-empty name resolves case-insensitively to the built-in registry, the spelling
must be the canonical registry name and its count/interval payload must equal that definition;
unregistered names and anonymous definitions remain valid custom analysis scales. Key-state
equality compares tonic spelling (ignoring octave), accidentals, mode name, note count, and interval
sequence; description is persisted non-semantic metadata, and a mode-only change is therefore a
real state transition.

**Invariant**: At least one entry in bar 1; a conventional initial entry is at
`ScoreTime(1, Beat(0,1))`. Every standard-mode tonic/mode/fifths triple is coherent.

### 2.5 TimeSignatureMap

**Definition 2.5.1**. The *TimeSignatureMap* is a piecewise-constant function from bar number to time signature.

**Representation**: An ordered sequence of entries:

| Field | Type | Description |
|-------|------|-------------|
| `bar` | `u32` | Bar number where this time signature takes effect |
| `time_signature` | `TimeSignature` | The time signature (from Theory Spec §9.2) |

The time signature persists until the next entry. Every bar between two consecutive entries has the time signature of the earlier entry.

**Derived property**: The *duration of a measure* is `Beat(numerator, denominator)` in Sunny's single whole-note coordinate. Thus 4/4 is `Beat(1,1)`, while 6/8 and 3/4 are both `Beat(3,4)`. There is no alternate internal Beat-unit convention.

At a MIDI or Live boundary, the corresponding quarter-note count is `4 × Beat`: both 6/8 and 3/4 therefore span 3.0 host beats. This conversion is an adapter operation; it does not change the stored Beat value.

`TimeSignature` itself is construction-safe: groups are non-empty and positive, their sum is a
representable positive numerator, and the denominator is a positive power of two. JSON readers use
the checked grouped-metre factory, so malformed grouped metre is rejected before a `Score` can be
published. S5 consequently validates map placement/order and score range; it does not rescan an
impossible malformed `TimeSignature` object.

**Grouped metre**: Metrical feel is preserved in the stored group partition rather than inferred
again from measure duration. The deterministic flat-signature factory uses groups of three when
the numerator is at least six and divisible by three, and otherwise uses one group per notated
pulse; an explicit partition may override that default identity. This distinction affects:
- Default beam grouping (§4.10): simple metres beam by quarter-note beats; compound metres beam by dotted-quarter-note beats.
- Tempo interpretation: "♩ = 120" in 3/4 differs from "♩. = 120" in 6/8, even though both measures have the same duration in quarter notes.

The TempoMap's `beat_unit` field (§2.3) resolves this ambiguity by explicitly specifying which note value carries the beat.

**Invariant**: At least one entry at bar 1.

### 2.6 SectionMap

**Definition 2.6.1**. The *SectionMap* annotates the formal structure of the work as a hierarchical set of labelled time spans.

**Representation**: A tree of *Section* nodes:

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<Section>` | Unique section identifier |
| `label` | `SectionLabel` | Structural label |
| `start` | `ScoreTime` | Start position (inclusive) |
| `end` | `ScoreTime` | End position (exclusive) |
| `children` | `Vec<Section>` | Nested subsections |
| `form_function` | `Option<FormFunction>` | Formal role annotation |

**SectionLabel**: A string from a controlled vocabulary, extensible by the user. Standard labels include:

*Large-scale*: `Introduction`, `Exposition`, `Development`, `Recapitulation`, `Coda`, `Codetta`.

*Sectional*: `A`, `B`, `C`, `A'`, `B'`, `Bridge`, `Transition`, `Retransition`.

*Popular form*: `Verse`, `Chorus`, `Pre-Chorus`, `Hook`, `Outro`, `Interlude`, `Solo`, `Breakdown`, `Drop`, `Build`.

*Internal*: `Phrase`, `Antecedent`, `Consequent`, `Presentation`, `Continuation`, `Cadential`.

**FormFunction** enumeration: `Expository`, `Developmental`, `Transitional`, `Cadential`, `Introductory`, `Closing`, `Parenthetical`. This classifies the *function* of a section independently of its label, allowing formal analysis across different naming conventions.

**Invariants**:
- Every node has a nonempty label, a valid non-empty half-open span within the score (whose end may
  be the terminal bar line), and an in-domain optional FormFunction.
- Sections at the same nesting level do not overlap.
- Every child's time span is contained within its parent's time span.
- The map may be empty or partial; no implicit section is invented for uncovered score time.

### 2.7 RehearsalMark

**Definition 2.7.1**. A *RehearsalMark* is a named navigation point.

| Field | Type | Description |
|-------|------|-------------|
| `position` | `ScoreTime` | Location in the score |
| `label` | `String` | Display label (e.g., "A", "B", "1", "Allegro con fuoco") |

---

## 3. Part Level

### 3.1 Part

**Definition 3.1.1**. A *Part* represents a single instrumental line in the score.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<Part>` | Unique part identifier |
| `definition` | `PartDefinition` | Instrument and playback configuration |
| `measures` | `Vec<Measure>` | Ordered sequence of measures |
| `part_directives` | `Vec<PartDirective>` | Part-scoped performance instructions |
| `hairpins` | `Vec<Hairpin>` | Gradual dynamic changes spanning multiple events (§4.5.5) |

**Invariant**: `measures.len() == score.metadata.total_bars`. Every part has exactly one Measure for every bar in the score, even if that measure contains only rests.

### 3.2 PartDefinition

**Definition 3.2.1**. The *PartDefinition* specifies the instrument, its capabilities, and its rendering configuration.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `name` | `String` | Display name (e.g., "Violin I", "Alto Saxophone", "Pad Synth") |
| `abbreviation` | `String` | Abbreviated name for condensed scores (e.g., "Vln. I", "A. Sax.") |
| `instrument_class` | `InstrumentClass` | Classification for orchestration logic |
| `transposition` | `Interval` | Written-to-sounding transposition interval (see §3.2.4) |
| `clef` | `Clef` | Default clef |
| `range` | `PitchRange` | Playable range (soft limits) and comfortable range (hard limits) |
| `articulation_vocabulary` | `Vec<ArticulationType>` | Articulations this instrument supports |
| `staff_count` | `u8` | Number of staves (1 for most; 2 for piano, organ, harp) |
| `staff_clefs` | `Vec<Clef>` | Optional initial clef for each staff, ordered top-to-bottom; empty means `clef` applies to every staff |
| `rendering` | `RenderingConfig` | DAW-specific rendering parameters |

`staff_count` is positive. If `staff_clefs` is populated it has exactly `staff_count` entries.
No instrument-type heuristic invents a lower-staff clef: an older or compact definition with an
empty vector uses the stored default `clef` on every staff.

#### 3.2.1 InstrumentClass

**Definition 3.2.2**. Hierarchical instrument classification for orchestration reasoning.

```
InstrumentClass
 ├── Strings
 │    ├── BowedStrings { Violin, Viola, Cello, DoubleBass }
 │    └── PluckedStrings { Harp, Guitar, Lute, Banjo, Mandolin }
 ├── Woodwinds
 │    ├── Flute, Piccolo, AltoFlute, BassFlute
 │    ├── Oboe, EnglishHorn, BassOboe
 │    ├── Clarinet, BassClarinet, EbClarinet
 │    ├── Bassoon, Contrabassoon
 │    └── Saxophone { Soprano, Alto, Tenor, Baritone }
 ├── Brass
 │    ├── FrenchHorn, Trumpet, Cornet
 │    ├── Trombone, BassTrombone
 │    └── Tuba, Euphonium
 ├── Percussion
 │    ├── Pitched { Timpani, Xylophone, Marimba, Vibraphone, Glockenspiel, TubularBells, Celesta }
 │    └── Unpitched { SnareDrum, BassDrum, Cymbals, Triangle, Tambourine, TamTam, Castanets, WoodBlock }
 ├── Keyboard
 │    ├── Piano, Harpsichord, Organ, Accordion
 │    └── Synthesiser { subclass: String }
 ├── Voice
 │    ├── Soprano, MezzoSoprano, Alto
 │    └── Tenor, Baritone, Bass
 └── Electronic
      ├── Sampler { library: String }
      ├── DrumMachine { kit: String }
      └── Effect { type: String }
```

The `InstrumentClass` determines default behaviours for orchestration: which instruments can double at the unison (same class), which octave doublings are conventional (e.g., flutes doubling violins an octave above), which instruments blend well in a section, and which dynamic ranges are available.

#### 3.2.2 PitchRange

**Definition 3.2.3**. The playable pitch range of an instrument.

| Field | Type | Description |
|-------|------|-------------|
| `absolute_low` | `SpelledPitch` | Lowest physically producible pitch |
| `absolute_high` | `SpelledPitch` | Highest physically producible pitch |
| `comfortable_low` | `SpelledPitch` | Low end of the comfortable playing range |
| `comfortable_high` | `SpelledPitch` | High end of the comfortable playing range |

Pitches outside the comfortable range but within the absolute range are valid but generate a warning. Pitches outside the absolute range are invalid and generate an error.

#### 3.2.3 Clef

**Definition 3.2.4**. Standard clef types:

| Clef | Middle Line Pitch | Common Instruments |
|------|------------------|--------------------|
| `Treble` (G2) | B4 | Violin, flute, oboe, trumpet, right hand piano |
| `Bass` (F4) | D3 | Cello, bassoon, trombone, tuba, left hand piano |
| `Alto` (C3) | C4 | Viola |
| `Tenor` (C4) | C4 (different line) | Cello (high passages), bassoon (high passages), trombone |
| `Percussion` | — | Unpitched percussion |
| `Tab` | — | Guitar tablature |

Clef changes within a part are represented as Direction events (§4.7).

#### 3.2.4 Transposition

**Definition 3.2.5**. For transposing instruments, the `transposition` field specifies the interval from *written pitch* to *sounding pitch*. The Score IR stores all pitches at concert pitch (sounding). The transposition is applied during notation rendering to produce the written part.

Examples:
- Clarinet in B♭: transposition = descending major second (−2 semitones, −1 diatonic step). Written C4 sounds B♭3.
- Horn in F: transposition = descending perfect fifth (−7, −4). Written C4 sounds F3.
- Concert pitch instruments (violin, flute, piano): transposition = (0, 0).

**Invariant**: Note events in the Score IR always represent sounding (concert) pitch. Transposition is a rendering concern, not a storage concern.

#### 3.2.5 RenderingConfig

**Definition 3.2.6**. DAW-specific configuration for compiling this part to an Ableton Live track.

| Field | Type | Description |
|-------|------|-------------|
| `instrument_preset` | `Option<String>` | Requested instrument/plugin preset metadata; the current public-LOM compiler reports it as unsupported rather than guessing a load operation |
| `midi_channel` | `u8` | MIDI channel (1–16) |
| `articulation_map` | `Map<ArticulationType, ArticulationMapping>` | How each articulation projects to note-local performance fields and, where admitted, MIDI controls |
| `expression_cc` | `u8` | CC number for expression/dynamics (default: CC 11) |
| `pan` | `Option<f32>` | Stereo pan position (−1.0 to 1.0) |
| `group` | `Option<String>` | Ableton group track assignment |

**ArticulationMapping** variants:

| Variant | Description |
|---------|-------------|
| `Keyswitch(SpelledPitch)` | Send a velocity-127, one-tick keyswitch note at the articulated note's tick |
| `CC(u8, u8)` | Send a CC message (controller number, value) |
| `VelocityLayer(u8, u8)` | Map to a velocity range (min, max) |
| `NoteDurationScale(f32)` | Scale note duration by a factor (e.g., staccato = 0.5) |
| `ProgramChange(u8)` | Send a program change message |
| `Combined(Vec<ArticulationMapping>)` | Multiple simultaneous mappings |

The mapping is a validated tagged union: only the selected variant's payload is normative.
Keyswitch pitch and every MIDI value must be in 0–127; velocity-layer bounds must be in 1–127
with `min ≤ max`; a duration scale must be finite and positive; and `Combined` must be non-empty.
Trees are bounded to depth 16 and 1024 total nodes. `Combined` children execute in stored order.
An explicit mapping replaces the default articulation stage; when no mapping exists, the fixed
duration/velocity treatment in §9.4–9.5 remains the deterministic fallback. A flat `NoteEvent`
projection applies the representable `VelocityLayer` and `NoteDurationScale` effects and diagnoses
every requested Keyswitch, CC, or ProgramChange child because its record has no control-event
field. The stored IEEE-754 duration factor is converted to its exact binary rational at that
boundary and must fit `Beat`; an exact-rational target does not silently substitute an
approximation.

### 3.3 PartDirective

**Definition 3.3.1**. A *PartDirective* is a scoped instruction that applies to an entire part over a time range.

| Field | Type | Description |
|-------|------|-------------|
| `start` | `ScoreTime` | Effective from |
| `end` | `ScoreTime` | Effective until |
| `directive` | `DirectiveType` | The instruction |
| `divisi_count` | `u8` | Required as at least two for `Divisi`; otherwise non-normative |

**DirectiveType** examples: `Mute`, `Solo`, `ConSordino`, `SenzaSordino`, `Pizzicato`, `Arco`, `Divisi(u8)`, `Tutti`, `Tacet`, `ColLegno`, `SulPonticello`, `SulTasto`, `HalfPedal`, `SustainingPedal`, `UnaCorda`, `TreCorde`.

These directives express normative rendering and notation intent over a non-empty half-open Part
span. MusicXML and LilyPond preserve both boundaries as exact visible instruction text, including
the Divisi count. MIDI maps only the lossless MIDI 1.0 switch ranges: SustainingPedal to CC64 and
UnaCorda to CC67, value 127 at start and 0 at end. Other technique/host state remains explicit
non-deployment evidence; the compiler does not invent keyswitches, continuous half-pedal depth,
prior-state restoration, devices, or Live automation. S20 owns range and conditional-payload
validity.

The reverse Corpus MIDI profile may construct `SustainingPedal`/`UnaCorda` spans only from
completed CC64/CC67 switch transitions on the sole source note channel. MIDI switch thresholds
are semantic, while this Score projection is canonical: non-0/127 source bytes are retained as
normalization evidence. It never manufactures an `ArticulationMapping` from a raw CC or Program
Change because the file contains no corresponding Score articulation identity.

---

## 4. Measure and Event Level

### 4.1 Measure

**Definition 4.1.1**. A *Measure* is a single bar within a part.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `bar_number` | `u32` | 1-indexed bar number |
| `voices` | `Vec<Voice>` | One or more voices within this measure |
| `local_key` | `Option<KeySignature>` | Local key signature override (courtesy/cautionary) |
| `local_time` | `Option<TimeSignature>` | Local time signature override |

**Invariants**:
- Measures in each Part are stored in strict score order and `measures[i].bar_number == i + 1`.
- `voices` is non-empty (at minimum, one voice containing rests).
- Positive-duration NoteGroup/Rest intervals explicitly tile the measure duration derived from the
  active time signature. Construction/mutation workflows materialise rests; validation does not
  invent implicit rests for a malformed stored document.

A local time signature is Part-specific notation and measure structure. MusicXML and LilyPond can
retain a duration-changing local bar. The current MIDI, generic NoteEvent, and Ableton performance
projections instead have one global ScoreTime-to-absolute-time function. They admit a local
signature only when its exact measure duration equals the active global duration; a different
grouping then remains notation metadata and increments `dropped_time_sig_events`. An unequal local
duration returns `TargetValueUnrepresentable` before producing performance events or contacting
Live, because placing the next Part bar on the global boundary would otherwise introduce an
unstated gap or overlap.

### 4.2 Voice

**Definition 4.2.1**. A *Voice* is a monophonic (or chordal) stream of events within a measure.
Multiple Voices may share a staff, while `staff_index` assigns each complete Voice to one of the
Part's staves for that measure. Cross-staff movement is representable at a measure boundary by a
later Voice with the same `voice_index` and a different staff; a mid-measure cross-staff beam is not
representable because staff ownership is not event-local.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `voice_index` | `u8` | 0-indexed voice number within the measure |
| `staff_index` | `u8` | 0-indexed owning staff within the Part |
| `events` | `Vec<Event>` | Ordered sequence of events |

**Invariant**: Voices are stored by unique, strictly increasing `voice_index` (indices may be
sparse after removal), and `staff_index < PartDefinition.staff_count`. Events are ordered by start
time. No two measured note/rest events within the same voice overlap in time (this is the
monophonic constraint within a voice; chords are represented as simultaneous notes within a
single event, not as overlapping independent notes). Point Directions and ChordSymbols may occur
within a measured span.

### 4.3 Event

**Definition 4.3.1**. An *Event* is the atomic unit of musical content. Events are sum-typed (tagged union):

```
Event
 ├── NoteGroup      — one or more simultaneous pitched notes
 ├── Rest           — a measured silence
 ├── ChordSymbol    — a harmonic annotation (lead-sheet style)
 └── Direction      — a performance instruction attached to a time point
```

All event types carry a common header:

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<Event>` | Unique event identifier |
| `offset` | `Beat` | Start time relative to measure start (Beat(0,1) = downbeat) |

**Invariant**: For any event in a measure with active time signature producing whole-note duration *D* (per §2.5): offset ∈ [Beat(0,1), *D*). An event's span (offset + duration for NoteGroup/Rest) must not exceed *D*.

### 4.4 NoteGroup

**Definition 4.4.1**. A *NoteGroup* represents one or more simultaneous notes sounding at the same onset and sharing the same rhythmic value. A single melodic note is a NoteGroup of size 1. A chord played by a single instrument (e.g., a piano chord) is a NoteGroup of size > 1. The group's grace state is homogeneous: it contains either only ordinary notes or only grace notes of one `GraceType`. Grace notes cannot carry duration ties.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `notes` | `Vec<Note>` | One or more notes (non-empty) |
| `duration` | `Beat` | Exact structural allocation; a tuplet member's written duration is this allocation multiplied by the cumulative `actual/normal` ratios |
| `tuplet_context` | `Option<TupletContext>` | Innermost enclosing tuplet, if any |
| `beam_group` | `Option<Id<BeamGroup>>` | Beam grouping identifier |
| `slur_start` | `bool` | Whether a slur begins here |
| `slur_end` | `bool` | Whether a slur ends here |

The two booleans encode one active slur identity per Voice. They can both be true on a shared
NoteGroup to stop an incoming slur and then start an outgoing slur; incoming-stop ordering is
normative. Overlapping/nested slurs need explicit identifiers that this profile does not store and
are rejected by S23 rather than assigned target numbers heuristically.

### 4.5 Note

**Definition 4.5.1**. A *Note* is a single pitched event with full performance metadata.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `pitch` | `SpelledPitch` | Concert pitch with enharmonic spelling |
| `velocity` | `Velocity` | Attack intensity |
| `release_velocity` | `u8` | Note Off intensity in `[0,127]`; canonical neutral default 64 |
| `articulation` | `Option<Articulation>` | Articulation marking |
| `dynamic` | `Option<Dynamic>` | Dynamic marking at this note |
| `ornament` | `Option<Ornament>` | Ornamental figure |
| `tie_forward` | `bool` | Whether this note is tied to the next occurrence of the same pitch |
| `grace` | `Option<GraceType>` | Grace note classification, if this is a grace note |
| `technical` | `Vec<TechnicalDirection>` | Instrument-specific performance instructions |
| `lyrics` | `Vec<LyricSyllable>` | Ordered verse-specific lyric syllables at this onset |
| `notation_head` | `Option<NoteHead>` | Non-standard notehead (diamond, cross, slash, etc.) |

Attack and release ownership follow sounding boundaries. For an ordinary untied note, both values
belong to that note. For a tie chain, the first segment owns attack velocity and the terminal
segment owns release velocity; intermediate release values are retained state that becomes
operative only if an edit makes that segment terminal. MIDI and Live compilation must consume the
same resolved terminal value. A flat performance target with no tie field, including generic
`NoteEvent`, must collapse the validated chain to one attack at the head, the exact summed
duration, and one release at the terminal segment; emitting the stored segments independently
would introduce false retriggers. Grace notes cannot carry ties and therefore own both boundaries.

#### 4.5.2 LyricSyllable

**Definition 4.5.2**. A *LyricSyllable* is one verse-specific text-underlay token at a NoteGroup
onset.

| Field | Type | Description |
|---|---|---|
| `text` | `String` | Non-empty syllable text |
| `verse` | `u16` | Positive verse number |
| `syllabic` | `LyricSyllabic` | `Single`, `Begin`, `Middle`, or `End` position in a word |
| `extend` | `bool` | This syllable continues across at least one following ordinary NoteGroup |

Lyrics are onset-scoped: only the first Note in a NoteGroup owns them. At one onset verse entries
are unique and strictly increasing. Within each `(Part, staff_index, voice_index, verse)` lane,
`Begin (Middle)* End` is the only multi-syllable word sequence; `Single` occurs outside an open
word. Lyrics do not attach to grace NoteGroups. An extended syllable is `Single` or `End`, spans at
least one later ordinary NoteGroup, and ends immediately before the next syllable in the same lane
or at the lane's last ordinary NoteGroup. This range algebra is sufficient to derive MusicXML
extend endpoints and LilyPond extender/skip tokens without storing target syntax.

#### 4.5.3 Velocity

**Definition 4.5.3**. Velocity is represented as both a semantic dynamic level and a numeric value, to separate compositional intent from rendering output.

| Field | Type | Description |
|-------|------|-------------|
| `written` | `Option<DynamicLevel>` | Semantic dynamic (if a dynamic marking is present at this note) |
| `value` | `u8` | Explicit MIDI attack velocity in 1–127; zero is the unresolved sentinel |

`Note.dynamic` and `VelocityValue.written` are coherent semantic carriers: when both are present R7
requires equality, and `Note.dynamic` has precedence. If neither is present, a nonzero numeric value
is explicit attack intent; zero delegates to the Voice's current semantic dynamic. Performance
compilation then applies any hairpin, articulation mapping/fallback, and orchestration balance
without mutating the stored value.
A semantic dynamic stored on a tied continuation does not create another attack, but it updates the
Voice's running dynamic state for later attacks after that continuation position.

#### 4.5.4 Articulation

**Definition 4.5.4**. Articulation types, grouped by family:

**Duration-affecting**:
- `Staccato` — shortened to approximately half the written duration
- `Staccatissimo` — shortened to approximately one quarter
- `Tenuto` — held for full written duration (or slightly beyond)
- `Portato` (mezzo-staccato) — between legato and staccato

**Accent-affecting**:
- `Accent` — emphasis (increased velocity)
- `Marcato` — strong emphasis (more than accent)
- `Sforzando` — sudden strong attack
- `ForzandoPiano` — strong attack followed by immediate piano

**Technique-specific**:
- `Fermata` — held beyond written duration (duration multiplied by a configurable factor, default 1.5–2.0)
- `Trill { interval: Interval }` — rapid alternation with a note at the given interval. *Note*: Trill also appears in the Ornament type (§4.5.7). When both an Articulation.Trill and an Ornament.Trill are present, the Ornament takes precedence for notation rendering (it carries richer detail including accidental specification). The Articulation.Trill form is retained for instruments that map trills to keyswitches or CC via ArticulationMapping, where the ornamental detail is not required
- `Mordent { inverted: bool }` — single alternation
- `Turn { inverted: bool }` — four-note figure
- `Tremolo { strokes: u8 }` — unmeasured repetition (1 = eighth-note, 2 = sixteenth, 3 = thirty-second)
- `Harmonic { type: HarmonicType }` — natural or artificial harmonic
- `GlissandoStart` / `GlissandoEnd` — continuous pitch slide between two notes

The legacy glissando articulation flags similarly encode at most one active glissando per Voice.
Chordal concurrent glissandi require numbered identities (as in MusicXML) and are outside this
profile; S23 rejects them.
- `SnapPizzicato` — (Bartók pizzicato) for strings
- `DownBow` / `UpBow` — bowing direction for strings
- `OpenString` / `Stopped` — for strings and brass
- `Muted` — with mute applied (if not already a PartDirective)
- `BendUp(cents: i16)` / `BendDown(cents: i16)` — pitch bend

Each articulation has a default rendering behaviour (§9) that can be overridden per instrument in the RenderingConfig.

#### 4.5.5 Dynamic

**Definition 4.5.5**. Dynamic markings.

**Instantaneous dynamics** (`DynamicLevel`):

| Level | Name | Default Velocity Range |
|-------|------|----------------------|
| `pppp` | Pianississimo | 8–15 |
| `ppp` | Pianissimo | 16–31 |
| `pp` | Piano | 32–47 |
| `p` | Piano | 48–63 |
| `mp` | MezzoPiano | 64–79 |
| `mf` | MezzoForte | 80–95 |
| `f` | Forte | 96–111 |
| `ff` | Fortissimo | 112–119 |
| `fff` | Fortississimo | 120–125 |
| `ffff` | Fortissississimo | 126–127 |
| `fp` | FortePiano | Attack at f, sustain at p |
| `sfz` | Sforzando | Accent at ff |
| `sfp` | SforzandoPiano | Accent at ff, sustain at p |
| `rfz` | Rinforzando | Local reinforcement |

The velocity ranges above are defaults; they are configurable per score and per instrument to account for the differing dynamic ranges of, say, a piccolo versus a bass drum.

**Gradual dynamics** (`Hairpin`):

| Field | Type | Description |
|-------|------|-------------|
| `start` | `ScoreTime` | Where the hairpin begins |
| `end` | `ScoreTime` | Where the hairpin ends |
| `type` | `HairpinType` | `Crescendo` or `Diminuendo` |
| `target` | `Option<DynamicLevel>` | Target dynamic at the end (if specified) |

Hairpins are stored on the Part (not on individual notes) because they span multiple events. During velocity resolution, any active hairpin linearly interpolates between the starting dynamic and the target (or, if no target is specified, increases/decreases by one dynamic step).

#### 4.5.6 GraceType

**Definition 4.5.6**. Grace notes are pre-beat or on-beat ornamental notes.

| Variant | Description | Rendering |
|---------|-------------|-----------|
| `Acciaccatura` | Crushed grace note (slashed stem) | Very short, before the beat |
| `Appoggiatura` | Leaning grace note (no slash) | Takes time from the following note |

Grace notes have a positive written duration that also reserves a structural allocation in the
Score timeline and therefore participates in S2/S3 measure topology. This is a deliberate,
tractable source-model convention; it is not a claim that MusicXML or LilyPond grace notation
advances the notated cursor. Notation compilers preserve the grace class and written value but
delegate playback convention to the notation target.

The MIDI/Live sounding policy is exact:

- `Appoggiatura` sounds from the beginning of the allocation for its full duration.
- `Acciaccatura` sounds for `min(allocation_duration, Beat(1,32))` and is right-aligned to the end
  of the allocation.

The fixed `Beat(1,32)` bound is one thirty-second note because Sunny `Beat` uses whole-note units.
Articulation duration scaling is applied after grace resolution. The same resolved interval is
presented to `compile_to_midi`, the NoteEvent projection, SMF conversion, and Ableton clip-note
translation before each target's explicit tick, exact-rational, or floating conversion. The policy
never creates a negative start time, including at score start.

#### 4.5.7 Ornament

**Definition 4.5.7**. Ornament types:

| Ornament | Description |
|----------|-------------|
| `Trill { interval: Interval, accidental: Option<i8> }` | Rapid alternation with upper note |
| `Mordent` | Single rapid alternation with lower note |
| `InvertedMordent` | Single rapid alternation with upper note |
| `Turn` | Upper–main–lower–main |
| `InvertedTurn` | Lower–main–upper–main |
| `Shake` | Baroque-era trill starting on upper note |
| `Arpeggio { direction: ArpeggioDirection }` | Rolled chord (`Up`, `Down`, `None`) |

### 4.6 Rest

**Definition 4.6.1**. A *Rest* is a measured silence.

| Field | Type | Description |
|-------|------|-------------|
| `duration` | `Beat` | Duration of the rest |
| `visible` | `bool` | Whether to render in notation (false for implicit padding rests) |
| `tuplet_context` | `Option<TupletContext>` | Innermost enclosing tuplet, if any |

### 4.7 Direction

**Definition 4.7.1**. A *Direction* is a performance instruction or notation annotation that does not have pitch or duration but is attached to a time point within a measure. Directions do not consume time; they are zero-duration metadata events.

**DirectionType** variants:

| Category | Examples |
|----------|---------|
| Text | `Espressivo`, `Dolce`, `Con fuoco`, `Cantabile`, arbitrary text |
| Tempo text | `Allegro`, `Adagio`, `Più mosso`, `Meno mosso`, `A tempo`, `Rubato` |
| Clef change | New clef for this part from this point forward |
| Coda / Segno | Navigation symbols for repeat structures |
| Barline | Double barline, final barline, repeat signs |
| Ottava | `8va`, `8vb`, `15ma`, `15mb` with start/end positions |
| Pedal | Sustain pedal down/up, half-pedal, una corda, tre corde |
| Breath | Breath mark or caesura |

The programmed Direction payload is closed:

| Field | Type | Owning variant |
|---|---|---|
| `text` | `Option<String>` | Required and non-empty for `Text` and `TempoText` |
| `new_clef` | `Option<Clef>` | Required for `ClefChange` |
| `ottava_shift` | `i8` semitones | Required as exactly `+12`, `-12`, `+24`, or `-24` for `OttavaStart`; zero otherwise |

Missing required payload is an S19 Error. Populating one of these fields on another variant is an
S19 Warning: the document remains compilable, but the irrelevant value is not part of that
Direction's semantics.

Ottava state is Staff-scoped; sustain-pedal state is Part-scoped because one physical damper pedal
affects the instrument even though its notation point is visually anchored by a Voice on one Staff.
S23 permits at most one active ottava per Staff and one active sustain-pedal state per Part,
requires every ottava to stop, and rejects orphan stops, overlap, empty spans, or same-time
endpoints whose order or pedal-change meaning is not stored. A final unmatched `PedalDown` is the
one deliberate exception: it means sustain through the final bar line, matching LilyPond's
documented omission of the terminal `\sustainOff`. If both Direction points and a
`SustainingPedal` PartDirective encode the pedal, their inferred ranges must agree exactly.

### 4.8 ChordSymbol

**Definition 4.8.1**. A *ChordSymbol* is a harmonic annotation in lead-sheet or Roman numeral form, attached to a time position. It does not produce sound directly; it annotates the harmonic context for the purpose of analysis, improvisation, or agent reasoning.

| Field | Type | Description |
|-------|------|-------------|
| `root` | `SpelledPitch` | Root of the chord (letter + accidental) |
| `quality` | non-empty `String` | Chord quality; registered spellings have native target mappings and other values remain exact display text |
| `bass` | `Option<SpelledPitch>` | Slash bass note, if different from root |
| `roman` | `Option<non-empty String>` | Optional display spelling such as `V65`; never parsed to invent semantics |
| `extensions` | `Vec<non-empty String>` | Legacy/free-form display residuals such as `add9` or `♯11` |
| `numeral` | `Option<ChordNumeral>` | Structured scale-degree root, alteration, and explicit numeral key |
| `inversion` | `Option<u16>` | Zero-based inversion (`0` is root position) |
| `degrees` | `Vec<ChordDegree>` | Ordered structured add/alter/subtract operations |

`ChordNumeral.root ∈ {1,…,7}` and `ChordNumeral.alteration` is an integer semitone count in
`i8`. Its required key stores `fifths ∈ [-7,7]` and one closed MusicXML-compatible mode:
`Major(0)`, `Minor(1)`, `NaturalMinor(2)`, `MelodicMinor(3)`, or `HarmonicMinor(4)`.
`ChordDegree.value` is a positive `u16`, its alteration is an integer semitone count in `i8`, and
its type is `Add(0)`, `Alter(1)`, or `Subtract(2)`.

The separately useful playback/engraving `root` must agree with a structured numeral. Let *f* be
the numeral key's fifths and let *q = f* for Major or *q = f + 3* for every minor mode. The tonic
is `from_line_of_fifths(q)`. The expected root letter advances `numeral.root - 1` diatonic steps;
its exact accidental is the difference between that target letter's natural pitch and the tonic
plus the corresponding interval from
Major `[0,2,4,5,7,9,11]`, Minor/Natural Minor `[0,2,3,5,7,8,10]`, Melodic Minor
`[0,2,3,5,7,9,11]`, or Harmonic Minor `[0,2,3,5,7,8,11]`, plus the stored alteration. The
calculation carries an octave when the diatonic step wraps from B to C. S25 rejects any exact
letter/accidental contradiction, including a mod-12-equivalent but wrongly spelled root. This makes the root shared by notation,
analysis, and playback a checked projection rather than a second independent assertion.

The tractable profile owns one harmony chord per event and integer-semitone alterations. The
compact MusicXML reader and Corpus ingester reconstruct this same root-or-numeral, key, kind,
inversion, bass, ordered-degree, and exact measure-offset algebra. MusicXML stacked harmony chords
and fractional `xs:decimal` microtonal alterations are outside this source domain and are rejected,
not claimed as preserved. They must be added as typed source constructs before an importer or
compiler may admit them.

### 4.9 TupletContext

**Definition 4.9.1**. A *TupletContext* groups a set of events under a tuplet bracket.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<TupletContext>` | Shared among all events in this tuplet |
| `actual` | `u8` | Number of nominal rhythmic units in the tuplet (*m* in *m*:*n*); not the number of events |
| `normal` | `u8` | Number of nominal rhythmic units in the normal span (*n* in *m*:*n*) |
| `normal_type` | `Beat` | Written duration of a nominal rhythmic unit |
| `nested_in` | `Option<Id<TupletContext>>` | Parent tuplet if nested |

Each measured event stores only its innermost context; following `nested_in` yields an acyclic
outermost-to-innermost chain. All descendant measured events form one contiguous span. Members
may have unequal durations, and a child context may replace part of an enclosing context's span.
The rhythmic ratio does not impose an event or child-context cardinality.

Let `A(t)` be the ancestors of context *t*. Its structural span is

`normal(t) × normal_type(t) × ∏[a ∈ A(t)] normal(a) / actual(a)`.

The exact `duration` values of all events tagged with *t* or its descendants sum to that span.
Thus `duration` is already the target-independent structural/sounding allocation; a notation
compiler must not scale it again. An event's written duration is its stored allocation multiplied
by the product of `actual/normal` for every context in its outermost-to-innermost chain.
`normal_type` defines the ratio's counting unit rather than every member's note/rest type.

**Example**: A quarter-note triplet in 4/4 has `actual = 3`, `normal = 2`, and
`normal_type = Beat(1,4)`. Its structural span is `Beat(1,2)` and three equal members each store
`Beat(1,6)`. If a 3:2 eighth-note child replaces one outer member, its local quarter-note normal
span is scaled by the outer 2:3 ratio to `Beat(1,6)`; three equal child events each store
`Beat(1,18)`.

**Mixed-duration example**: A 3:2 eighth-note triplet may contain a written quarter and an eighth.
Its `normal_type` is `Beat(1,8)`, but its two events store `Beat(1,6)` and `Beat(1,12)` and together
occupy `Beat(1,4)`. This is the mixed triplet illustrated by the
[MusicXML notation tutorial](https://www.w3.org/2021/06/musicxml40/tutorial/notation-basics/#tuplets).
Splitting a member into shorter notes or rests preserves validity when the exact contiguous
structural span and shared context remain valid; event count is not an additional constraint.

### 4.10 BeamGroup

**Definition 4.10.1**. A *BeamGroup* identifies one explicit primary beam in one Voice and
Measure. The implemented runtime does not synthesize groups from metre; callers either store an
explicit group or leave beaming to the target's defaults.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<BeamGroup>` | Unique beam group identifier |
| `event_ids` | `Vec<Id<Event>>` | Ordered events under this beam |
| `beam_breaks` | `Vec<u8>` | Reserved secondary-beam metadata; must be empty in the compilable profile |

For beaming, an event's *written duration* is its stored duration multiplied by the cumulative
`actual/normal` ratio of its complete tuplet context chain, or its stored duration when it is not
in a tuplet. Arithmetic or context-chain failures make the group invalid. A valid group has a
globally unique ID and at least two members. Its member IDs are unique, strictly ordered as stored
in the Voice, and contiguous among
that Voice's measured events. Each member is a NoteGroup or Rest with a positive written duration
shorter than a quarter note. A NoteGroup member is ordinary rather than grace material and carries
the matching `beam_group` back-reference; every NoteGroup back-reference names exactly the local
group that contains it. A Rest has no redundant back-reference.

`beam_breaks` cannot be given a lossless meaning: a boundary index does not say which of the up to
eight MusicXML beam levels breaks or whether a surviving beamlet is a forward or backward hook,
and it does not provide LilyPond's per-stem left/right beam counts. S14 therefore rejects every
non-empty value. A future secondary-beam model requires explicit per-level, per-side state rather
than another exporter heuristic.

### 4.11 NoteHead

**Definition 4.11.1**. Non-standard notehead shapes for special notation.

| Variant | Description | Common Usage |
|---------|-------------|--------------|
| `Normal` | Standard filled or open notehead | Default |
| `Diamond` | Diamond-shaped | Natural harmonics (strings) |
| `Cross` | X-shaped | Ghost notes (percussion), dead notes (guitar) |
| `Slash` | Slash through stem | Rhythmic notation, strumming patterns |
| `Triangle` | Triangle-shaped | Special percussion notation |
| `CircleX` | Circled X | Specific extended techniques |
| `Square` | Square-shaped | Early music notation, spoken text |
| `Cue` | Sounding note rendered at cue size | Reduced or editorial context; not a silent cue event |

### 4.12 TechnicalDirection

**Definition 4.12.1**. Instrument-specific performance instructions that affect rendering but are not articulations in the traditional sense.

| Variant | Applicable Instruments | Description |
|---------|----------------------|-------------|
| `Fingering(Vec<u8>)` | Keyboard, strings, woodwinds | Suggested fingering numbers |
| `StringNumber(u8)` | Strings | Which string to play on (I–IV for violin) |
| `Position(u8)` | Strings | Left-hand position |
| `BowingPattern(String)` | Bowed strings | Specific bowing notation |
| `BreathMark` | Wind, voice | Breath or phrasing mark |
| `Slide { direction: SlideDirection }` | Guitar, trombone | Slide into or out of a note |
| `HammerOn` | Guitar | Left-hand tap onto higher fret |
| `PullOff` | Guitar | Left-hand release to lower fret |
| `Bend { cents: i16 }` | Guitar | String bend by specified interval |
| `Vibrato { speed: VibratoSpeed }` | Strings, wind, voice | Vibrato intensity indication |
| `Caesura` | All | Full stop / grand pause |

**SlideDirection**: `Into`, `OutOf`, `Ascending`, `Descending`.

**VibratoSpeed**: `Slow`, `Normal`, `Fast`, `None`.

---

## 5. Temporal Coordinate System

### 5.1 ScoreTime

**Definition 5.1.1**. *ScoreTime* is the primary temporal coordinate within the Score IR.

| Field | Type | Description |
|-------|------|-------------|
| `bar` | `u32` | 1-indexed bar number |
| `beat` | `Beat` | Whole-note offset within the bar (`Beat(0,1)` is the start; `Beat(1,4)` is the second quarter-note boundary in 4/4) |

**Ordering**: ScoreTime values are totally ordered. (*b*₁, *β*₁) < (*b*₂, *β*₂) iff *b*₁ < *b*₂, or *b*₁ = *b*₂ and *β*₁ < *β*₂.

### 5.2 AbsoluteBeat

**Definition 5.2.1**. *AbsoluteBeat* is the cumulative beat position from the beginning of the score, computed by summing measure durations:

AbsoluteBeat(ScoreTime(*b*, *β*)) = ∑ᵢ₌₁^{*b*−1} duration(measure *i*) + *β*

This is a monotonically increasing function of ScoreTime and provides a single-axis coordinate suitable for alignment across parts.

### 5.3 RealTime

**Definition 5.3.1**. *RealTime* is the clock time (in seconds) from the beginning of the piece. Let *B*<sub>q</sub> be the effective quarter-note BPM derived from the TempoEvent's `beat_unit`. Because AbsoluteBeat is measured in whole notes:

RealTime(*t*) = ∫₀^{AbsoluteBeat(*t*)} (240 / *B*<sub>q</sub>(τ)) dτ

For piecewise-constant tempos, this is a sum of linear segments. For linear tempo transitions (accelerando/ritardando), the integral has the following closed-form solution.

**Closed form for linear tempo interpolation**: If effective quarter-note BPM varies linearly from *B*₁ to *B*₂ over a span of *d* whole notes (where *B*₁ ≠ *B*₂), the real-time duration of that span is:

Δt = 240 · *d* · ln(*B*₂ / *B*₁) / (*B*₂ − *B*₁)

This follows from integrating `240 / Bq(τ)` over τ ∈ [0, *d*]. In the degenerate constant-tempo case, the formula reduces to Δt = 240 · *d* / *B*₁.

**Implementation note**: The natural logarithm and division introduce floating-point arithmetic into an otherwise exact-rational pipeline. Implementations should compute RealTime in double precision (IEEE 754 binary64) and accept the resulting representation error, which is bounded by machine epsilon (≈ 2.22 × 10⁻¹⁶) per segment. Accumulated error across *n* segments is bounded by *n* · ε · max(Δt), which remains negligible for practical scores (fewer than 10⁴ segments).

RealTime is the clock-domain boundary for render/scheduling clients. MIDI and the current Ableton
adapter retain exact ScoreTime/AbsoluteBeat until PPQ or quarter-note target conversion; they do not
replace musical coordinates with seconds. Internal Score IR operations remain in ScoreTime.

### 5.4 Tick Time

**Definition 5.4.1**. *TickTime* is the discretised time coordinate used by MIDI and DAW transport, measured in pulses per quarter note (PPQ). Given a PPQ resolution *R* (default: 480):

TickTime(*t*) = AbsoluteBeat(*t*) × 4*R*

Since AbsoluteBeat is an exact rational, TickTime is also an exact rational. However, MIDI files require integer ticks; the Score IR compiler rounds to the nearest integer tick and tracks the accumulated rounding error to prevent drift.

**Invariant**: Cumulative rounding error in tick conversion never exceeds 0.5 ticks over any bar boundary.

**Rounding algorithm**: For each measure, the compiler computes the exact rational tick count for the measure duration and rounds it to the nearest integer. Individual event ticks within the measure are computed by distributing the rounding residual using Bresenham-style error accumulation:

1. Compute the exact tick position for each event: *t*_exact = AbsoluteBeat(event) × 4*R*.
2. Round to the nearest integer: *t*_rounded = round(*t*_exact).
3. At each bar boundary, compute the cumulative error *e* = *t*_rounded − *t*_exact.
4. If |*e*| > 0.5, adjust the final event in the measure by ±1 tick to bring the bar boundary error within tolerance.

This ensures that bar boundaries align exactly (or within 0.5 ticks) with their theoretical positions, preventing drift over long scores.

---

## 6. Harmonic Annotation Layer

### 6.1 Purpose

The *HarmonicAnnotationLayer* provides a global harmonic analysis of the score, separate from and parallel to the note content of the individual parts. This layer enables an agent to reason about harmony at the chord-progression level without extracting and re-analysing the pitch content of every part for every query.

The layer is *derived* (computable from the note content plus the key signature map) but *persisted* (stored explicitly and maintained under edits) because re-derivation is expensive for large scores and because certain harmonic interpretations involve heuristic choices that should not be silently recomputed.

### 6.2 HarmonicAnnotation

**Definition 6.2.1**. A *HarmonicAnnotation* is a harmonic analysis entry.

| Field | Type | Description |
|-------|------|-------------|
| `position` | `ScoreTime` | Where this harmony takes effect |
| `duration` | `Beat` | Duration of this harmonic region |
| `chord` | `ChordVoicing` | The chord (from Theory Spec) |
| `key_context` | `KeySignature` | The local key for Roman numeral analysis |
| `roman_numeral` | `String` | Roman numeral analysis in `key_context` |
| `function` | `HarmonicFunction` | Functional classification (see below) |
| `secondary_function` | `Option<String>` | e.g., "V/V", "viio/vi" |
| `non_chord_tones` | `Vec<NonChordToneAnnotation>` | Identified passing tones, suspensions, etc. |
| `cadence` | `Option<CadenceType>` | If this annotation participates in a cadence |
| `confidence` | `f32` | Confidence of the analysis (1.0 = certain, < 1.0 = heuristic) |

**HarmonicFunction** enumeration (from Theory Spec §6.1):

| Value | Abbreviation | Description |
|-------|-------------|-------------|
| `Tonic` | T | Stability and rest; includes I, vi, iii |
| `Predominant` | PD | Departure from tonic, preparation for dominant; includes IV, ii, vi (contextual) |
| `Dominant` | D | Tension seeking resolution to tonic; includes V, vii° |
| `Ambiguous` | — | Function cannot be determined with confidence (e.g., chromatic or non-functional harmony) |

The `Ambiguous` value is used when the harmonic context is insufficient to assign a clear function — common in chromatic passages, modal interchange, and non-functional harmony. The `confidence` field provides a continuous measure alongside this discrete classification.

**Invariant 6.2.2 (annotation payload closure).** Each persisted annotation has a non-empty,
strictly ascending MIDI-note voicing, non-empty quality and Roman-numeral labels, closed enum
values, a non-empty optional secondary-function label, and finite confidence in `[0, 1]`. If the
quality resolves through the chord-quality registry, the voicing's pitch-class set is exactly the
registered interval set transposed from `root`; octave doublings are permitted. `inversion` is a
valid registry interval index and the lowest note has that indexed pitch class. A quality outside
the registry is retained for non-tertian/analytical material only in root position, with its bass
pitch class equal to `root`, because no interval algebra exists from which to validate another
inversion. `key_context` satisfies the same complete key-identity rule as a KeySignatureMap entry.

**Invariant 6.2.3 (section-harmony derivation).** `set_section_harmony` treats each entry's spelled
root octave as the exact source register and constructs every registered quality member or rejects
the operation. An optional slash bass must be an exact chord member; it becomes the lowest note
and determines inversion, while every other member is raised as needed to form one strictly
ascending voicing. Non-chord slash basses fail closed because `ChordVoicing` has no independent
pedal-bass carrier. Roman numeral and function are derived under the KeySignatureMap entry active
at that chord's position, including its stored mode intervals; a region-wide key guess or
fifths-sign heuristic is not admissible. Candidate construction and validation precede the one
atomic document commit.

### 6.3 NonChordToneAnnotation

**Definition 6.3.1**. Identifies a non-chord tone in the score.

| Field | Type | Description |
|-------|------|-------------|
| `event_id` | `Id<Event>` | The event containing the non-chord tone |
| `note_index` | `u8` | Which note within the NoteGroup |
| `type` | `NonChordToneType` | Classification |

**NonChordToneType**:

| Type | Definition |
|------|-----------|
| `PassingTone` | Stepwise connection between two chord tones, on a weak beat |
| `NeighborTone` | Step away from and back to the same chord tone |
| `Suspension` | Held from previous chord, resolves down by step |
| `Retardation` | Held from previous chord, resolves up by step |
| `Appoggiatura` | Approached by leap, resolves by step, on a strong beat |
| `EscapeTone` | Approached by step, left by leap in the opposite direction |
| `Cambiata` | Step–leap–step pattern |
| `Anticipation` | Arrives early (on weak beat) before the chord it belongs to |
| `Pedal` | Sustained pitch (typically bass) held through chord changes |

### 6.4 CadenceType

From the Theory Spec (§6.6): `PerfectAuthentic`, `ImperfectAuthentic`, `Half`, `Plagal`, `Deceptive`, `PhrygianHalf`.

### 6.5 Consistency with Note Content

**Invariant**: The harmonic annotation layer is *consistent* with the note content if, for each annotation entry, the pitch classes present in all parts at that time position are explainable as chord tones or classified non-chord tones. When note content is edited, the annotation layer is marked *stale* for the affected region. Re-analysis of stale regions is an explicit operation (not automatic), preserving agent control over interpretation.

**Stale region tracking**: The Score IR maintains a set of *stale intervals* — a list of non-overlapping `(ScoreTime, ScoreTime)` pairs representing regions where the harmonic annotation may no longer match the note content. When a mutation affects notes in a region [*a*, *b*), the interval [*a*, *b*) is added to the stale set (merging with any overlapping existing intervals). When the agent explicitly re-analyses a region, the corresponding interval is removed from the stale set. The same mechanism applies to the OrchestrationLayer. The stale set is persisted in serialisation and survives round-trip save/load.

---

## 7. Orchestration Layer

### 7.1 Purpose

The *OrchestrationLayer* annotates the textural role of each part at each point in the score. This metadata is not audible; it encodes the compositional reasoning behind the orchestration. An agent uses this layer to understand *why* a passage is orchestrated as it is, enabling informed modification. Reorchestrating a passage without this layer requires re-inferring the textural roles from the note content, which is an underdetermined problem.

### 7.2 TexturalRole

**Definition 7.2.1**. A *TexturalRole* classifies a part's function within a passage.

| Role | Description |
|------|-------------|
| `Melody` | Carries the primary melodic line |
| `CounterMelody` | An independent secondary melodic line |
| `HarmonicFill` | Provides harmonic support (sustained chords, pads) |
| `BassLine` | Provides the lowest voice and harmonic foundation |
| `RhythmicOstinato` | Provides a repeating rhythmic pattern |
| `Doubling { source: Id<Part>, interval: Interval }` | Doubles another part at a specified interval |
| `Pedal { pitch: SpelledPitch }` | Sustains a single pitch through harmonic changes |
| `Obligato` | An essential accompanying figure that is part of the compositional identity |
| `Dialogue { partner: Id<Part> }` | Call and response with another part |
| `Tutti` | Part of a tutti texture (all instruments playing) |
| `Tacet` | Silent |
| `Solo` | Featured solo instrument |
| `Accompagnato` | Accompanying a solo or melody |

### 7.3 OrchestrationAnnotation

**Definition 7.3.1**. An orchestration annotation entry.

| Field | Type | Description |
|-------|------|-------------|
| `part_id` | `Id<Part>` | Which part this annotation applies to |
| `start` | `ScoreTime` | Start of the annotated region |
| `end` | `ScoreTime` | End of the annotated region |
| `role` | `TexturalRole` | The part's role in this region |
| `texture` | `Option<TextureType>` | Global texture classification |
| `dynamic_balance` | `Option<DynamicBalance>` | Relative prominence (foreground, middle ground, background) |

**TextureType**: `Monophonic`, `Homophonic`, `Polyphonic`, `Heterophonic`, `Homorhythmic`, `Antiphonal`, `Fugal`, `Chorale`, `Unison`, `Melody_Accompaniment`.

**DynamicBalance**: `Foreground`, `MiddleGround`, `Background`. This tells the rendering compiler to adjust relative velocity or expression levels so that foreground parts are prominent, middleground parts are present but subordinate, and background parts are soft and sustained.

---

## 8. Reduction and View System

### 8.1 Purpose

A *View* is a derived, read-only projection of the Score IR that presents the content in a compressed or filtered form. Views do not contain independent data; they are computed from the full score on demand and are invalidated when the underlying content changes.

Views serve two functions: they provide an agent with simplified representations for reasoning about high-level structure (without processing 40 independent parts), and they provide a human with conventional condensed formats for review.

### 8.2 View Types

#### 8.2.1 PianoReduction

**Definition 8.2.1**. A *PianoReduction* collapses all parts into two staves (treble and bass), preserving harmonic content while simplifying texture.

**Algorithm** [H]:
1. For each harmonic region (from the HarmonicAnnotationLayer), collect all sounding pitch classes across all parts.
2. Distribute pitches to treble and bass staves based on register (above/below middle C as default split point, adjustable).
3. Reduce to a maximum of 4 voices per staff.
4. Preserve the melody voice (identified from the OrchestrationLayer) as the top voice of the treble staff.
5. Preserve the bass line as the bottom voice of the bass staff.
6. Eliminate octave doublings.

**Output**: A two-part Score IR with staves labelled "Treble" and "Bass" and a piano instrument assignment.

#### 8.2.2 ShortScore

**Definition 8.2.2**. A *ShortScore* collapses parts by instrument family, producing a condensed score with one staff per family (or two for families spanning treble and bass range).

| Staff | Source Parts |
|-------|-------------|
| Woodwinds (treble) | Flutes, oboes, clarinets (upper register) |
| Woodwinds (bass) | Bassoons, bass clarinet |
| Brass (treble) | Horns, trumpets |
| Brass (bass) | Trombones, tuba |
| Percussion | All percussion |
| Strings (treble) | Violins, violas |
| Strings (bass) | Cellos, double basses |

The collapsing algorithm preserves the highest and lowest sounding pitch in each family at each time point and marks omitted inner voices with cue-sized noteheads or an ellipsis annotation.

#### 8.2.3 PartExtract

**Definition 8.2.3**. A *PartExtract* isolates a single part from the score, including:
- All events from the selected part.
- Key and time signatures from the global maps.
- Rehearsal marks and section labels.
- Cue notes from other parts at significant entries (configurable: after rests of *n* or more bars).
- Transposition applied if the part is a transposing instrument.

This is the standard format for producing individual performer parts.

#### 8.2.4 HarmonicSkeleton

**Definition 8.2.4**. A *HarmonicSkeleton* reduces the score to its chord progression, stripping all melodic and rhythmic detail.

**Output**: A sequence of (ScoreTime, duration, ChordSymbol) entries, equivalent to a lead sheet without melody. This is the most compressed view and is useful for harmonic analysis, reharmonisation, and formal structure review.

#### 8.2.5 RegionView

**Definition 8.2.5**. A *RegionView* extracts a bounded region of the score (specified by a Region address, §1.2) as an independent Score IR fragment, preserving all layers (notes, harmony, orchestration) within the region.

The current view unit is the complete set of source bars intersected by the Region. At the selected
start-bar downbeat, the view materialises active time signature, key, and exact instantaneous
effective-quarter tempo as new origin entries. If the boundary intersects a Linear tempo interval,
the new tempo is Immediate and the retained destination's Linear duration is shortened to the exact
remaining interval. Later source changes retain their relative positions. This history
normalisation guarantees that a valid source produces S4/S5/S6/S24-coherent global maps instead of
an orphaned incoming transition.

This is useful for focused work on a single section, and for comparing parallel passages (e.g., the exposition and recapitulation of a sonata).

---

## 9. Compilation

### 9.1 Compilation Model

*Compilation* is the deterministic transformation of a validated Score IR document into a rendering target. Each target has its own compiler. All compilers read the Score IR and produce output; no compiler modifies the Score IR.

```
Score IR ──┬──→ AbletonCompiler ──→ Ableton Live Session (via LOM bridge)
           ├──→ MidiCompiler ──→ Typed MIDI event data
           ├──→ MusicXmlCompiler ──→ MusicXML (.musicxml)
           └──→ LilyPondCompiler ──→ LilyPond (.ly)
```

The Ableton target becomes the audio-rendering environment after the aggregate project compiler
configures Score, Timbre, and Mix in the same session using the identity and ordering rules in
`sunny-project-model.md`. Sunny does not currently expose a separate PCM `AudioCompiler`.

### 9.2 AbletonCompiler

**Definition 9.2.1**. The *AbletonCompiler* emits the supported Live Object Model operations for a Score IR and returns a compilation summary. The summary distinguishes transport success from completeness: `success` means all emitted operations were accepted and every evidence payload required from a real current-protocol transport was well formed; `complete` means no requested feature was outside the supported LOM surface and no observed scalar diverged from its request.

Before emitting mutations, the production transport performs the versioned target-profile handshake
defined in the bridge contract (`remote_script/Sunny/bridge_contract.json`). The compiler records the observed Live version and
capability states in its result. A missing, malformed, or internally contradictory profile is a
pre-mutation protocol error, not a reason to guess. Recording transports must supply an explicit
modelled profile and cannot obtain capabilities merely by omitting one.
The Part-to-track ordinal range must also fit the bridge's canonical non-negative signed-index
domain. An unrepresentable range fails before the target profile, scene count, or any mutation is
requested. Compilation summary counters are 64-bit and therefore do not inherit that addressing
limit.
The initial meter must additionally inhabit Live's documented value domain: numerator 1–99 and
denominator in {1, 2, 4, 8, 16}. Sunny's target-independent metre algebra remains broader; an
initial value outside the target subset returns 4112 (`TargetValueUnrepresentable`) before target
access. Later meter events are retained as unsupported automation and therefore need not satisfy
the current-value subset.
The initial tempo is normalised to effective quarter-note BPM before deployment and must inhabit
Live's documented 20–999 Song.tempo range. A target-independent marking such as half-note = 60
therefore deploys as Song.tempo = 120. An out-of-range value returns 4112 before target access.

**Compilation steps**:

1. **Initial globals**: After the target-value preflight, set the Song's effective-quarter initial tempo and time signature. `tempo_events_requested` and `time_signature_events_requested` count source map points, while each corresponding `*_written` count is currently one. Later tempo transitions/events and meter events remain in the Score IR and produce capability warnings because the admitted public LOM surface exposes current Song scalars but no tempo- or meter-automation authoring operation.

   Live's documented Song and Clip metre state contains only numerator and denominator scalars.
   `time_signature_groupings_requested` therefore counts source partitions that differ from
   Sunny's deterministic reconstruction for the same flat signature, while
   `time_signature_groupings_written` is zero. Such a request emits a capability warning and makes
   compilation incomplete even when scalar write/readback proves the requested flat value. A Live
   `5/8` observation cannot establish whether source intent was `3+2`, `2+3`, or another partition.

   Aggregate schema-14 evidence also observes exact Boolean Song transport-running, count-in,
   Arrangement Record, Session Overdub, Automation Arm, `arrangement_overdub`, and `overdub`
   state. The Song gate requires the six recording/count-in modes false; ordinary playback is
   exported independently and does not by itself invalidate structural deployment. Sunny does not
   change these controls, and it does not interpret the public integer `session_record_status`
   because the current reference gives no value mapping.

   After ensuring Scene 0 exists, set/read back its name from `Score.metadata.title`, then set/read
   back both its tempo-override and meter-override enable flags as false. The single Session row
   identifies the full Score while sections remain CuePoints. The Scene contract then uses Song
   state when launched; an existing target Scene cannot silently replace the globals installed
   above.

   The final structural observer additionally retains exact Boolean `Scene.is_triggered` for every
   Scene, not only Scene 0. Aggregate Song evidence requires the complete vector to be false. This
   excludes a documented pending/blinking Scene launch only at those sequential reads; compilation
   issues no Scene fire/stop command and proves neither launch completion, recording preferences,
   atomicity with Clip/Track predicates, future stability, playback, nor sound.

   Global and nonredundant Part-local keys contribute to
   `key_signature_events_requested`; the current bridge writes zero and reports the residual
   because a safe Live 12 scale write also needs observed tuning compatibility and an accepted
   scale-name mapping.

   The complete `ScoreTuning` contributes one `tuning_definitions_requested` record and is retained
   verbatim as `requested_tuning`; `tuning_definitions_written` is currently zero. Live 12.1's
   writable `live_set tuning_system` object is therefore recognized but not guessed at. The public
   LOM reference specifies the semantic dictionary properties and says that `note_tunings` holds
   one array, but does not close the member schemas needed for an exact write/readback protocol.
   Non-standard source tuning adds a capability warning. Even canonical 12-TET remains an explicit
   incomplete target obligation because an existing Live Set may have a different active tuning.

2. **Track creation**: For each Part, create an Ableton MIDI track via the LOM bridge and set its name. Set and immediately read back exact Boolean `Track.arm = false` and `Track.implicit_arm = false`, exact integer `Track.mixer_device.crossfade_assign = 1`, Live's public “neither A nor B” value, and exact integer `panning_mode = 0` before setting `RenderingConfig.pan` through `Track.mixer_device.panning.value`. Live documents armed Tracks as admitting monitored input and recording/overdub participation; `implicit_arm` is its second public arm state used by Push. Live's Split Stereo mode (`1`) replaces the single Stereo Pan operation with independent left/right positions and is not a representation of Sunny's one pan scalar. Aggregate schema-14 evidence requires both arm states to remain false, the final crossfade assignment to remain 1, and final panning mode 0. It also requires the final Track's public `is_frozen` state to be false, because a frozen Session clip plays its freeze audio rather than the current MIDI/device state, and explicitly observed null Group Track membership: a parent Group can add a summing mixer/effects and commonly changes the Part's route without changing its own Track facts. This does not materialise or verify Sunny GroupBuses. The current snapshot schema additionally requires the created Part to retain exact `(has_audio_input, has_midi_input) = (false, true)`, plus exact selected input-routing type/channel dictionaries that are full-dictionary members of Live's corresponding advertised option arrays. This proves the expected MIDI-input Track class and a Live-valid current selection, not the semantic identity or neutrality of the selected source; Sunny performs no input-route mutation and never interprets a presentation label such as “No Input.” The same gate requires exact floating `input_meter_level = output_meter_level = 0.0` plus `input_meter_left = input_meter_right = output_meter_left = output_meter_right = 0.0`. Live documents the first pair as one-second hold peaks and the latter four as smoothed momentary peaks available on audio-output Tracks, with additional GUI load when observed. Sunny reads each once and exports separate hold, stereo-momentary, and conjunctive verdicts. These verdicts exclude bounded metered activity only at the sequential final reads; they do not prove continuous event/signal absence, monitoring state, interface behavior, or sound. Every normal Track also retains its complete ordered ClipSlot vector. Generated Part evidence requires exact slot cardinality/order and Clip occupancy agreement, and every slot to be non-Group, status-zero/not playing, not recording/not will-record, and not triggered. Stop Button presence remains observed target state rather than Sunny intent. This closes a pending action on a later empty cell at the sequential final read, without firing/stopping a slot or proving atomicity, future stability, recording preferences, playback, signal, or sound. On Live 11+, the same Track snapshot retains the exact cardinality of the `arrangement_clips` list, and the generated Part gate requires zero; earlier modeled targets retain null and cannot verify this obligation. Live 11 already has take lanes. The `take_lanes` cardinality is retained and required to be zero only on API-qualified Live 12+; below Live 12 this adapter carries null because API exposure remains unqualified, and take-lane absence remains unverified. Count zero proves the requested Arrangement-content absence proposition without serializing private Clip objects. On Live 12+ it also removes the Take Lane branch that Ableton documents as audible under Audition Mode, whose state is not exposed in the public LOM. Neither count proves atomicity, future stability, playback, or sound. Disarming does not establish the monitoring selector: the current public Track LOM omits the mode that Ableton documents as capable of suppressing clip output, so `monitoring_state_observed` and `clip_output_not_suppressed_by_monitoring_verified` remain false even for an unfrozen/disarmed Track gate. Arm/input-class/selection/meter-hold/Session/Arrangement/Take-Lane/crossfade/pan-mode closure does not prove external-input neutrality, output routing, Return-path neutrality, continuous signal, or sound.

   Staff topology is notation layout inside a Part, not playback routing. `staff_count`,
   `staff_clefs`, and `Voice.staff_index` therefore do not create additional Live tracks or clips;
   every sounding Voice in the Part is compiled into that Part's single clip. Track identity remains
   `PartId`, never staff ordinal.

3. **Clip generation**: Create one Session View clip in slot zero per Part, spanning the greater of the structural score duration and the latest compiled musical note end. `ClipSlot.create_clip` specifies neither meter inheritance nor activator/loop/marker/launch/groove/envelope defaults. Set/read back start marker 0, end marker equal to that compiled length, `looping = false`, and `muted = false`, making the Score a finite active unlooped structural interval. Ableton defines an unlooped Clip's played range by these start/end markers; the independently stored loop brace is dormant and unobserved under the false loop invariant. Admitting a looped projection would require separate `loop_start`/`loop_end` intent and evidence. The current snapshot schema independently requires public audio false/MIDI true/Arrangement false identity and read-only `end_time` equality with the requested End Marker. On Live 11+, it also requires Session true, and on API-qualified Live 12+ Take-Lane false (below Live 12 the unavailable take-lane identity remains null and cannot verify false), then sets/reads back `launch_mode = 0` (Trigger), `launch_quantization = 1` (None), `legato = false`, floating `velocity_amount = 0.0`, and the null `groove` object. An associated groove can non-destructively alter timing, random offsets, and velocity despite exact stored note dictionaries; generated clips therefore require final `has_groove = false`. The public Clip LOM exposes no Follow Action state even though Ableton documents Follow Actions that can stop, restart, or launch clips and override loop/region behavior. Sunny neither fires the Clip nor observes a playback trace, so the structural interval and bounded launch tuple do not prove one-shot playback. Final generated-Clip evidence first requires the complete occupied Session-slot set on its Track to equal exactly `{0}`; target-owned content in any other slot remains observed but makes the projection incomplete. The current snapshot schema retains exact Boolean `is_playing`, `is_recording`, `is_overdubbing`, `is_triggered`, and `will_record_on_start` state. A generated Clip verifies only when all five are false at the sequential final observation; Sunny does not issue a stop, claim atomicity, or guarantee that a later user/controller action cannot change them. Before inserting notes, call the public all-envelope clear through the exact no-argument adapter and require immediate Boolean `has_envelopes = false`; aggregate evidence requires the same final state. Clip Envelopes can otherwise automate/modulate mixer or device controls and carry MIDI controller data. Also set/read back the initial Score signature on each created Clip before inserting notes. Within the clip, each NoteGroup compiles to one or more Live note dictionaries:
   - Pitch: `midi(note.pitch)` from the Theory Spec.
   - Velocity: `note.velocity.value` (resolved; see §9.5).
   - Start: TickTime of the event offset within the clip, after grace timing resolution when applicable.
   - Duration: Grace timing resolution followed by articulation adjustment (§9.4).
   - Mute: the compiled note activator state.
   - Probability: adapter-owned floating `1.0` (always play).
   - Velocity deviation: adapter-owned floating `0.0` (no random attack deviation).
   - Release velocity: floating projection of the Score-owned terminal `release_velocity`.

   `notes_requested` counts the ordinary note dictionaries produced by the shared MIDI projection;
   `notes_written` counts dictionaries in successful `add_new_notes` calls. The distinction remains
   visible when a target below Live 11 preserves note intent but cannot admit the call.
   Each nonempty Part call also creates a `note_deployments` record. A real transport requires the
   exact list of unique integer note IDs documented as the call's return value and verifies its
   cardinality against the requested batch; missing, duplicate, or wrong-sized evidence is a
   protocol error. It then calls `get_notes_by_id` with those IDs and an exact selected return list
   containing ID plus pitch/start/duration/velocity/mute, adapter-owned floating probability
   1.0 and velocity deviation 0.0, and Score-owned floating release velocity. The insertion dictionary explicitly
   sends the same deterministic values. The response must contain the same unique
   ID set and exact record schema. On Live 11.1+ it also calls `get_all_notes_extended` with the
   same selected fields and requires the same complete record multiset, proving no additional or
   missing ordinary notes. On Live 11.0 the existing `get_notes_extended` API queries all pitches
   `[0,128)` and note starts in `[0, generated_clip_end)` quarter-note beats. Selection follows note
   starts, including notes whose durations cross the query end, rather than clip markers. Its
   finite `observed_time_span` and false `entire_clip_population_observed` explicitly leave
   outside-range notes unobserved; matching inserted IDs/properties do not verify a complete batch. Requested and observed eight-property tuples are
   compared as multisets, so no undocumented return-order correspondence is assumed. A
   well-typed, consistent property difference is retained as unverified and warned; extra/missing
   notes, inconsistent queries, or malformed evidence are protocol errors. Recording
   transports use `recorded_only`, return no IDs/readback, and increment no executed, returned-ID,
   verified-batch, or verified-note counter.
   `pitch` is the Score's integer note index; its intended frequency comes from `ScoreTuning`, not
   intrinsically from 12-TET. On Live 12, an active Set tuning can reinterpret that index. The current
   snapshot schema observes the version-available Song scale tuple and every documented global
   TuningSystem property, preserving its four dictionaries exactly but without interpreting their
   undocumented members. It still cannot map that target payload to `ScoreTuning` or observe target
   track/device bypass/support conditions. Nonempty Live-12 note deployment therefore carries an explicit
   audible-pitch warning; neither requested nor written count proves frequency equivalence.

4. **MIDI preparation**: The existing MIDI compiler resolves velocity, duration, temporal conversion, and supported articulation data before LOM translation. The Ableton phase does not make musical decisions.

5. **Section markers**: Top-level Section starts compile through the bridge's `sunny_set_cue` adapter. The adapter uses Song's current-time cue method, preserves the playhead, and renames an existing cue instead of toggling it away. Nested nodes remain explicit residuals because CuePoints are flat and time-keyed: projecting a child that shares its parent's start would overwrite rather than preserve both labels. Results expose total/projected/unprojected section-node counts and a capability warning when hierarchy is retained only in the IR.

Initial Song tempo/signature, Scene-0 name/override flags, each created Clip's finite active
unlooped marker/loop/activator state, Live-11+ bounded launch tuple and null groove association,
and signature, track name, optional pan, and clip name are generic property writes. Each produces a
`property_deployments` record containing path, property,
requested value, optional observed value, and `verified`. A real transport must return typed
set/readback evidence; missing or malformed evidence is a protocol error. A different but
well-typed observed value is retained, makes that record unverified, and produces a completeness
warning. A recording transport retains the command plan with null observations and cannot claim
verification. Structural calls—track creation, clip creation, note insertion, and cue adaptation—
remain call-outcome evidence and are not promoted to scalar-state verification. In particular,
`notes_written` means accepted by the adapter call and returned IDs prove creation cardinality.
The separate all-envelope clear retains requested/executed/verified counts and one per-Part
deployment with requested absence, optional observed `has_envelopes`, action, and verdict. A real
transport requires the exact one-Boolean response; a recording transport claims only intent.
Paired insertion-ID and version-appropriate population readback verifies Sunny's five
Score-derived note properties and three fixed adapter properties within the observed domain.
Only whole-population access verifies exact Clip-note-set cardinality. Each deployment exposes
its requested tuple multiset, `observed_notes`, `properties_verified`,
`entire_clip_population_observed`, optional `observed_time_span`, `notes_verified`, and
`note_batches_verified`; the complete-batch verdict additionally requires whole-population
coverage and is therefore derivable from its
stored proposition and observation rather than trusted opaquely. It still is not
audible-output proof or evidence about per-note MPE expression.
Clearing and finally observing no Clip groove closes one documented playback transformation. The
bounded launch tuple separately removes launch-mode, clip-quantization, Legato-offset, and
launch-velocity scaling ambiguity, but the public Clip LOM exposes no Follow Action state.
`launch_behavior_verified` therefore does not imply `follow_actions_observed`,
`one_shot_playback_verified`, rendered timing, or rendered velocity. The device chain, latency,
routing, scheduling, and audio output also remain outside the stored-note proposition. Ableton's
UI additionally supports launch-time MIDI Clip bank/sub-bank/Program Change, but the public Clip
LOM exposes no corresponding state and clip creation promises no neutral default. Therefore
`midi_bank_program_state_observed` and `program_change_suppression_verified` remain false even when
Device identity and mapped parameters verify.
The all-envelope clear plus immediate and final `has_envelopes = false` closes the independently
tractable public Clip-envelope layer. It does not author automation and does not establish absence
of MPE note expression, control/modulation outside the Clip, or audible behavior.
Ableton separately documents note-owned Pitch, Slide, and Pressure MPE envelopes, but the current
public Clip note dictionaries expose no curves or expression-clear operation. Therefore aggregate
generated-Clip evidence retains false `mpe_note_expression_state_observed` and
`mpe_note_expression_neutrality_verified`; the selected ordinary note-property verdict cannot
imply neutral expression.
Current-protocol project evidence can retain each top-level Device's reported sample/millisecond
latency, but those values do not close delay compensation/monitoring mode, Track Delay, routing,
buffers/drivers, external hardware, or an audible reference event; they therefore do not change
this false timing verdict.
An offline transport labels the batch `recorded_only`; an observed pre-Live-11 target labels it
`unsupported`. Neither action carries created IDs or observed tuples.
For aggregate execution, each inserted batch's stored tuples and IDs become a distinct final-state
obligation: after all compiler phases and the structural post snapshot, a second closed complete-note
query must retain the exact ID set and property multiset. That sequential postcondition does not
change this Score compiler's insertion-time counters or imply atomic observation.

**Live capability boundary**:

- Clip note insertion targets Live 11+ `Clip.add_new_notes`. For an observed older target, the
  compiler still creates representable structure but skips note calls and reports incompleteness.
- Arbitrary instrument/plugin preset loading and Group Track creation/assignment are not public LOM operations. Requests for them produce warnings and no command.
- Custom velocity-layer and duration-scale mappings are already reflected in the musical notes
  sent to Live. Keyswitch, CC, and program-change events remain outside the current public-LOM
  deployment path; results expose separate `articulation_control_events_requested` and
  `articulation_control_events_written` counts and warn only when such an event was actually
  compiled. The written count is currently zero.
- Tempo and time-signature automation, CC automation, and clip automation-envelope authoring are
  not exposed by Sunny's closed public-LOM algebra. Their IR data is preserved and reported as
  incomplete; the public all-envelope clear only removes state from the fresh generated Clip and
  is not an authoring projection.
- Explicit time-signature grouping is likewise not a documented Song/Clip property. Flat
  numerator/denominator writes do not increment `time_signature_groupings_written`; distinct
  source grouping is retained and reported as an incomplete residual.
- Live 12.1 exposes writable current `TuningSystem` properties, and Score retains one complete
  128-note pitch function. The bridge still has no closed member schema for Live's semantic tuning
  dictionaries. The current snapshot schema retains the exact dictionary payloads opaquely and validates
  only their documented outer shapes; opaque observation supplies stale-plan evidence, not the
  semantic mapping required for safe mutation.
  `requested_tuning` and requested/written counts expose the exact residual; no tuning mutation or
  tuned-sound equivalence is claimed.
- Live 12 can apply a Set tuning to already stored MIDI note indices. The bridge preserves the
  integer note dictionaries and observes bounded global pitch context, including opaque target
  tuning dictionaries; their Score meaning, per-track bypass, and instrument/MPE support remain
  unobserved. Therefore Live-12 note compilation
  explicitly leaves audible pitch unverified even when `notes_written == notes_requested`.
- Native device-chain materialisation belongs to the Timbre compiler (§9 of the Timbre IR specification).
- Max for Live availability is not implied by a Live version and remains `unknown` unless a future
  deployable target can prove it independently.

The implemented Score-to-MIDI behaviours are:

1. **Articulation rendering**: A mapped articulation executes its validated mapping tree and
   produces keyswitch, control-change, program-change, velocity-layer, and/or duration-scale
   effects. An unmapped articulation uses the fixed fallback factors. Exact duplicate control
   events at the same tick are deduplicated. The generic NoteEvent projection applies the same
   note-local velocity and duration effects; it emits a location-bearing residual for mapping
   controls that the flat record cannot carry.

2. **Dynamic rendering**: Instantaneous dynamics and hairpins contribute to resolved note velocity. The current compiler does not emit CC automation lanes.

3. **Grace rendering**: Homogeneous grace groups use the exact allocation policy in §4.5.6. The
   generic NoteEvent projection and the MIDI/Ableton path share it; grace notes are neither omitted
   nor treated as ordinary full-slot notes.

4. **Tie rendering**: MIDI, generic NoteEvent, SMF, and Ableton performance projections collapse a
   validated ordinary tie chain to one physical attack/release interval. Exact segment durations
   are checked and summed, attack state comes from the head, release velocity comes from the
   terminal segment, and semantic dynamics encountered on consumed continuations still govern
   later attacks. Point events do not interrupt S7 measured-event adjacency. A target-domain drop
   counts the physical chain once rather than exposing its continuations as independent notes.

5. **Tempo rendering**: TempoMap entries compile to tick-positioned Set Tempo data. The compiler does not invent intermediate approximation events.

6. **Time and key signatures**: The corresponding maps compile to tick-positioned metadata records.
   Standard MIDI key-signature metadata retains the exact signed fifths count only in its
   −7…+7 domain and has only a major/minor flag. Wider signatures are counted and diagnosed as
   dropped metadata. Major/Ionian and minor/Aeolian map natively; other Sunny scales use the
   deterministic third-degree proxy and produce a compilation diagnostic rather than claiming
   that the analytical mode survived.

6. **Pedals, Part directives, and markers**: Paired PedalDown/PedalUp Directions and
   SustainingPedal PartDirectives compile to the same exact CC64 switch ranges; duplicate exact
   representations are deduplicated, while S23 blocks disagreement. A terminal PedalDown is closed
   with CC64 off at the score endpoint so state cannot leak beyond playback. UnaCorda compiles to
   CC67. Other directives remain semantic Score IR data with compilation diagnostics. Markers are
   not emitted by the current MIDI event compiler.

### 9.3 MidiCompiler

**Definition 9.3.1**. The implemented *MidiCompiler* produces a deterministic `CompiledMidi` event model. The C++ API returns that type and `score_compile_to_midi` returns its JSON representation; it does not return `.mid` file bytes. `score_export_midi` uses the infrastructure SMF reader/writer to return actual type-0 file bytes as base64, with the same compilation loss report; its PPQ domain is 1–32767, while the event model admits 1–65535.

Each note event retains both its requested MIDI channel and its source `PartId`. The Ableton compiler routes by `PartId`, so two parts that share a MIDI channel still deploy to separate tracks without duplicating or exchanging notes.

SMF identifies a Note Off only by channel and key, so a same-key note on one channel whose interval
ends strictly inside another's cannot be paired by a reader. The compiler emits such a nested
unison once, through the enclosing note, and reports the absorbed attack as a location-aware
residual. Identical intervals and intervals that share only their end remain distinct, because
first-in, first-out pairing reproduces both.

**Mapping**:

| Score IR Element | MIDI Event |
|-----------------|------------|
| Part | MIDI channel selected by its RenderingConfig |
| NoteGroup → Note | Note On / Note Off |
| TempoMap entry | Meta event: Set Tempo |
| TimeSignatureMap entry | Complete SMF Time Signature meta event (`nn`, encoded denominator exponent, `cc`, `bb`) when it fits the exact target profile |
| KeySignatureMap entry | Meta event retaining exact fifths only within SMF's −7…+7 domain; wider signatures are explicitly dropped; major/minor-family mode is native, all other analytical modes produce explicit proxy evidence |
| Dynamic and Hairpin | Resolved note velocity |
| GraceType | Resolved start and base sounding duration from §4.5.6 |
| Unmapped articulation | Default duration/velocity adjustment |
| `Keyswitch` mapping | One-tick Note On / Note Off pair |
| `CC` mapping | Control Change |
| `ProgramChange` mapping | Program Change |
| `VelocityLayer` mapping | Affine note-velocity remapping |
| `NoteDurationScale` mapping | Sounding-duration multiplier |
| Paired PedalDown/PedalUp Directions | CC64 value 127/0 at exact points; terminal open state receives off at score end |
| SustainingPedal PartDirective | CC64 value 127 at start and 0 at end |
| UnaCorda PartDirective | CC67 value 127 at start and 0 at end |
| Other PartDirective | No fabricated MIDI event; compilation diagnostic retains the unsupported intent |
| ScoreTuning | Canonical 12-TET/A4=440 is the admitted nominal MIDI baseline; every other complete pitch function is retained as one requested/zero-written tuning definition with a diagnostic |

**Resolution**: 480 PPQ (configurable).

SMF Set Tempo is a point-valued 24-bit unsigned microseconds-per-quarter word. Immediate and
MetricModulation targets emit one rounded word at their tick. A Linear destination is projected as
deterministic held-step samples on the PPQ lattice. Before 24-bit word quantisation, the qBPM hold
error is bounded by `max(0.5, |Qtarget-Qstart|/ramp_ticks)`: the second term is the irreducible
one-tick limit. Each ramp admits at most 4096 generated Set Tempo events; a rate outside the 24-bit
word domain, a source-event tick collision, or a ramp exceeding that bound returns
`InvalidMidiTempo` rather than weakening the guarantee or allocating without bound. The separate
tempo-word error at a sample is exactly
`|60,000,000 / round(60,000,000/Q) - Q|`.

An SMF Time Signature payload is `FF 58 04 nn dd cc bb`. `CompiledMidi` keeps the
denominator as its actual power-of-two value; the infrastructure writer converts it to the `dd`
base-two exponent. Sunny's current exact profile admits source numerators 1…255 and actual
denominators no greater than 128. The compiler checks those bounds before integer narrowing;
outside them the event is not emitted and the report retains the target loss. `bb` is 8 because
Sunny's score projection uses the ordinary eight notated 32nd notes per MIDI quarter.

Let the stored group sizes be \(g_1,\ldots,g_n\), let \(d\) be the denominator, and let
\(G=\gcd(g_1,\ldots,g_n)\). Since MIDI defines 24 clocks per quarter and therefore 96 per Sunny
whole note, the compiler emits

\[
cc = \frac{96G}{d}
\]

when that value is an integral byte in 1…255. This is the longest uniform click interval whose
grid contains every stored group boundary. If no such byte exists, `cc` falls back to the
conventional 24-clock quarter. The value has metronome semantics; it is not an SMF ordered-group
field. Consequently even an equal-group click interval does not count as a written grouping.
Canonical `6/8 = 3+3/8` emits `cc=36` without creating a non-default grouping obligation. An
explicit non-default `2+2/4` emits a matching `cc=48`, but its grouping remains requested and zero
written. `3+2/8` emits the common eighth-note grid `cc=12`; neither click value is promoted into a
claim that an SMF consumer received Sunny's source partition.

The MIDI report's time-signature event requested count covers every global map point plus every
nonredundant equal-duration Part-local residual. Its grouping requested count covers only
partitions that differ from Sunny's deterministic flat-signature constructor; grouping written is
zero because SMF has no ordered-group property. Either event or grouping shortfall makes
`has_drops()` true. Ableton uses the same definition of explicit grouping intent at its separate
flat-scalar boundary.

`CompilationReport` distinguishes two summary predicates. `has_drops()` is deliberately narrow:
it reports nonzero dropped-event counters and requested/written capability shortfalls.
`has_residuals()` is the target-completeness predicate and is true when `has_drops()` is true or
when any compilation diagnostic exists. The latter covers a usable approximation that retained
event cardinality but lost a semantic distinction, such as projecting a Dorian key through SMF's
major/minor bit. Both Boolean values and the complete diagnostic array are exposed in MCP report
JSON. Score and aggregate Ableton `complete` use `has_residuals()`; consumers need not reconstruct
that rule from counters and array cardinality.
The standalone Score MCP tools and aggregate Project MCP tools call the same private Infrastructure
encoder for this report. The Core model owns the evidence and summary algebra; Infrastructure owns
its JSON projection. No handler-local duplicate field list is an independent schema authority.

The infrastructure converter makes the channel basis explicit: `CompiledMidi` uses the
human-facing range 1–16 and SMF channel bytes use 0–15. It emits metadata first, then program
changes, control changes, same-tick note-offs, keyswitch note-ons, and musical note-ons. The SMF
reader/writer round-trips tempo, time signature, key signature, notes, CC, and program changes.

**Limitation**: MIDI does not preserve enharmonic spelling, source `PartId`, or the semantic name
of an articulation. A parsed note cannot be reclassified as a keyswitch without Score IR context.
It also has one global meter meta-event stream. Equal-duration measure-local regroupings remain
playable but enter `dropped_time_sig_events`; unequal-duration local bars are outside the shared
performance projection and fail before note timing is emitted. SMF key-signature metadata likewise
carries one global stream: a nonredundant `Measure.local_key` remains notation state and increments
`dropped_key_sig_events` instead of being silently conflated with the active global key. The Score
IR remains the source of truth.

### 9.4 Articulation Duration Resolution

**Definition 9.4.1** [C]. The sounding duration of a note is computed from its written duration and its articulation:

| Articulation | Duration Factor | Notes |
|-------------|----------------|-------|
| (none / legato) | `1/1` | Full written duration |
| Tenuto | `1/1` | Full duration, explicit |
| Portato | `3/4` | Between legato and staccato |
| Staccato | `1/2` | Half of written duration |
| Staccatissimo | `1/4` | Quarter of written duration |
| Accent | `1/1` | Duration unchanged; velocity affected |
| Marcato | `17/20` | Slightly separated |
| Fermata | `7/4` | Held beyond written duration |

These factors apply only when the note has no custom mapping for its articulation. A custom
mapping replaces this stage. Before any scaling, a note's tick duration is
`round(start + duration) - round(start)` on the absolute tick lattice rather than the rounded
duration alone, so each note ends exactly where its successor begins and a repeated key never
overlaps the next note. `NoteDurationScale` children multiply in `Combined` order; the scaled
MIDI tick duration is rounded to the nearest tick and clamped to at least one tick. Generic
`NoteEvent` instead multiplies its exact `Beat` duration by the exact rational represented by the
stored IEEE-754 float. It returns `TargetValueUnrepresentable` when that rational cannot fit the
signed 64-bit `Beat` domain and never uses an approximate rational conversion.

### 9.5 Velocity Resolution

**Definition 9.5.1** [C]. The MIDI velocity of a note is resolved by the following procedure:

1. **Base velocity**: Choose, in order, `Note.dynamic`, `Note.velocity.written`, an explicit
   numeric `Note.velocity.value` in 1–127, or the most recent semantic dynamic in the voice
   (default `mf`). Numeric zero is the unresolved sentinel and does not replace the semantic
   default.

2. **Hairpin adjustment**: If a Hairpin is active at the note's time position, interpolate linearly
   in exact cumulative whole-note time between the starting dynamic's velocity and the ending
   dynamic's velocity. If no target is stored, Crescendo/Diminuendo moves one step up/down the
   continuous `pppp`…`ffff` ladder; attack-shaped dynamics choose the nearest ladder level first.
   The starting dynamic is the chronologically latest semantic marking at or before the hairpin's
   start across the Part's voices (default `mf`). The Part-level dynamics query uses this same
   lookup, recognizing both `Note.dynamic` and `Note.velocity.written` with the former taking
   precedence. Earlier markings cannot override later ones through voice storage order. For
   simultaneous markings, the last marked note in canonical voice/event/note order wins. Numeric
   attack velocities do not change semantic state; ordinary non-hairpin defaults remain voice-local.

3. **Articulation adjustment**: If a custom mapping exists, it replaces this fixed adjustment.
   `VelocityLayer(min,max)` maps the resolved 1–127 velocity affinely into `[min,max]`. Otherwise:
   - Accent: velocity += 20 (clamped to 127).
   - Marcato: velocity += 30 (clamped to 127).
   - Sforzando: velocity = min(127, base + 40).
   - ForzandoPiano: attack velocity as sforzando. The MIDI note model has no independent
     post-attack amplitude segment.

4. **Dynamic balance adjustment**: If OrchestrationLayer assigns the part a DynamicBalance:
   - Foreground: no adjustment.
   - MiddleGround: velocity × 0.85.
   - Background: velocity × 0.70.

5. **Clamp**: Result is clamped to [1, 127].

**Order of operations**: Steps 1–4 are applied sequentially in the order listed. This ordering is significant: the hairpin interpolates over the *base* velocity (not the articulation-adjusted velocity), and the dynamic balance scales the *final* velocity (including articulation). The rationale is that articulation accents are compositional intent (written in the score) while dynamic balance is an orchestration concern (adjustable without altering the composition).

**Formal expression**: Let *v*₀ be the base velocity from step 1. Let *h*(*t*) be the hairpin interpolation factor at time *t* (a value in [0, 1] representing position within the hairpin). Let *a* be the articulation adjustment (additive). Let *b* be the dynamic balance factor (multiplicative). Then:

*v* = clamp((*v*₀ + *h*(*t*) · (*v*_target − *v*₀) + *a*) · *b*, 1, 127)

where *v*_target is the velocity at the hairpin's end dynamic.

### 9.6 MusicXmlCompiler

**Definition 9.6.1**. The *MusicXmlCompiler* produces MusicXML 4.0 output.

This compiler is the most information-preserving: MusicXML supports enharmonic spelling, articulations, dynamics, lyrics, clef changes, tuplets, beaming, slurs, ties, rehearsal marks, and multi-voice notation. The compilation is a structural mapping from the Score IR hierarchy to MusicXML elements.

The admitted MusicXML profile has no complete 128-note sounding-pitch carrier. Canonical
12-TET/A4=440 is its nominal baseline; any other `ScoreTuning` yields one requested/zero-written
tuning definition and a compilation diagnostic while notation pitch remains preserved.
The current notation profile also has no admitted release-velocity carrier. Every non-default
Score value therefore remains visible as a location-aware compilation diagnostic; it is not
reinterpreted as MusicXML playback metadata.
Both coherent semantic-dynamic carriers compile to the same `<dynamics>` direction. A numeric-only
attack velocity remains a location-aware residual: MusicXML's documented `dynamics` sound
suggestion is a percentage of the standard forte velocity 90, not an exact MIDI-byte field, so the
admitted notation profile neither drops the byte silently nor substitutes an approximate percent.
The document-level `SectionMap`, persisted harmonic and orchestration analysis, optional governing
tone row, and both stale-analysis region sets are outside this notation profile. Each populated
layer produces an explicit compilation diagnostic. In particular, Voice `ChordSymbol` events are
not silently substituted for the distinct global harmonic-analysis layer, and orchestration
`dynamic_balance` remains rendering policy rather than a MusicXML notation mark.

The separate compact `MusicXmlScore` reader/writer used by interchange tests and Corpus ingestion
has an explicit traditional-key profile. It preserves signed `fifths`, an optional value from
MusicXML 4's closed ten-value mode vocabulary, and a correctly mode-adjusted derived tonic.
Non-traditional `key-step`/`key-alter` signatures are rejected because that compact type does not
store the ordered diatonic alteration sequence; absence of `<fifths>` is never interpreted as zero.
Its source `<divisions>` values are parse-local scale factors normalized into exact `Beat`
durations. They are not retained as one misleading score-wide scalar, and the compact writer
computes the exact positive integer divisions required by the normalized durations. Its flat
`NoteEvent` adapter rejects non-default release velocity because that result type has no residual
report in which the discarded value could remain visible. Its harmony profile retains one
root-or-numeral chord at an exact measure-local `Beat`: a structured numeral requires its explicit
key and derives the canonical spelled root through the same core function used by S25; kind, bass,
inversion, and ordered integer-semitone degree operations are preserved. Cursor-relative offsets
are resolved against the live sequential cursor. Deprecated `function`, stacked chord sequences,
frames, non-primary staff assignment, fractional alterations, and unretained harmony attributes
are rejected rather than flattened.

| Score IR | MusicXML |
|----------|----------|
| Score | `<score-partwise>` |
| Part | `<part>` |
| Measure | `<measure>` |
| Voice | `<voice>` elements within a measure |
| NoteGroup (single note) | `<note>` |
| NoteGroup (chord) | First `<note>`, subsequent `<note>` with `<chord/>` |
| Rest | `<note>` with `<rest/>`; `visible=false` sets `note@print-object="no"` |
| SpelledPitch | `<pitch>` with `<step>`, `<alter>`, `<octave>` at written pitch: the concert pitch moved by the inverse of the Part's written-to-sounding interval |
| Duration (Beat) | Exact integer `<duration>`; a duration with no single note value is written as tied notes (or consecutive rests), each with its `<type>`, dots, and any time modification; a duration with no written form keeps its exact `<duration>` and is reported |
| GraceType | `<grace slash="yes|no">`; no MusicXML duration |
| Articulation | `<articulations>` or `<technical>` within `<notations>` |
| Ornament | Native child of `<ornaments>`, or `<arpeggiate>` |
| TechnicalDirection | Native `<technical>` / `<articulations>` child where lossless; otherwise `<other-technical>` plus a compilation diagnostic |
| NoteHead | Native `<notehead>` value; `Cue` uses `<type size="cue">` so the note remains sounding |
| LyricSyllable | Numbered `<lyric>` with native `<syllabic>`, `<text>`, and derived `<extend type="start|continue|stop">` state on the first chord note |
| Semantic dynamic (`Note.dynamic` or `VelocityValue.written`) | `<dynamics>` within `<direction>` |
| Numeric-only attack velocity | Location-aware residual; no exact admitted carrier |
| Hairpin | `<wedge>` within `<direction>` |
| Tie | `<tie>` and `<tied>`, with inferred stop and stored forward-start endpoints |
| Slur | `<slur>` within `<notations>` |
| TupletContext | Numbered `<tuplet type="start|stop">` boundaries at every nesting level + cumulative `<time-modification>` on each member; written `<type>` comes from its sounding allocation multiplied by every enclosing `actual/normal` ratio |
| BeamGroup | Exact primary `<beam number="1">begin|continue|end</beam>` sequence; S14 rejects under-specified secondary-break state before projection |
| KeySignature | Stored `accidentals` as `<fifths>` plus native major/minor/church `<mode>` within `<attributes>`; a transposing Part receives the written key |
| TimeSignature | `<time>` within `<attributes>`; unequal stored groups use additive `<beats>` text such as `3+2` |
| Clef | `<clef>` within `<attributes>` |
| Part staff topology | `<staves>` plus numbered initial `<clef>` elements; 0-based Sunny staff order becomes MusicXML's 1-based top-to-bottom numbering |
| Voice staff ownership | `<staff>` on notes, rests, forwards, harmonies, and Voice-scoped directions in multi-staff Parts |

For a chained slur on one NoteGroup, MusicXML's required musical-score order is emitted as
`type="stop"` followed by `type="start"`. S23 makes the omitted `number` safe by proving that no
concurrent slur/glissando/ottava identity needs disambiguation.
| TempoMap entry | Structured `<metronome>` preserving stored beat unit/rational rate plus point-valued effective-quarter `<sound tempo="...">` |
| Linear tempo transition | Explicit words at the previous event (the transition start), structured endpoint metronome mark, and a diagnostic that standard MusicXML sound tempo does not encode the continuous curve |
| MetricModulation | Exact old/new `<beat-unit>` relationship with `<metronome-relation>equals</metronome-relation>` plus the structured target rate |
| RehearsalMark | `<rehearsal>` within `<direction>` |
| PartDirective | Exact start text and explicit `end …` text within `<direction>` |
| Hierarchical SectionMap | Location-aware residual; no partial bookmark/rehearsal flattening is claimed |
| Transposition | `<transpose>` in `<attributes>` with both `<diatonic>` and `<chromatic>` written-to-sounding components |

When both `Articulation.Trill` and the richer `Ornament.Trill` occur on one Note, only the Ornament
is emitted. Its half-step, whole-step, or unison interval is encoded as MusicXML `trill-step`, and
its accidental is encoded as `accidental-mark` when MusicXML has an exact accidental value.
Unsupported interval or accidental values retain the visible trill and produce a compilation
diagnostic rather than being silently narrowed. The other first-class Ornament variants map to
their native MusicXML 4.0 elements; arpeggio direction maps to the `arpeggiate` direction
attribute.

Fingering, string number, bend, breath mark, and caesura have native lossless mappings. Sunny
technical variants whose model does not contain MusicXML's required linkage state (for example,
hammer-on, pull-off, and slide start/stop pairing), or for which MusicXML has no native element,
are preserved as textual `other-technical` extensions and reported as lossy-standard-mapping
diagnostics. The compiler does not invent start/stop semantics.

MusicXML's note child sequence is normative at this boundary. For ordinary notes, Sunny emits
duration, zero or more sound-tie endpoints, voice, any exactly derivable graphical type/dots, time modification,
notehead, notations, and lyric in schema order. Because Score IR stores only `tie_forward`, the
compiler derives the destination `stop` endpoint from the validated next measured event in the
same Voice; a middle note in a tie chain carries both stop and start. An ordinary tie may not
terminate on a grace note. Tuplet display markers occur only on the first and last member rather
than being repeated as starts on every member.

`Acciaccatura` emits `grace slash="yes"`; `Appoggiatura` emits `grace slash="no"`. MusicXML remains
a symbolic notation target: Sunny does not add target-specific steal-time percentages because its
exact allocation/sounding policy is already defined by §4.5.6 and no cross-engraver playback
equivalence is claimed. Because a MusicXML grace Note has no duration, the compiler follows each
grace NoteGroup with a Voice-scoped `forward` equal to its positive structural allocation. Thus the
following stored event retains its exact Score IR onset without falsely giving the grace Note a
MusicXML duration.

MusicXML has a mutable within-measure cursor. Measured NoteGroup/Rest events advance it; point
Direction and ChordSymbol events do not. A point emitted after an overlapping measured span carries
a signed `offset` equal to `event_offset - current_cursor`. Global TempoMap, RehearsalMark, and
Hairpin boundaries are emitted once per Part while the cursor is at the measure start, so their
offset is the absolute within-measure position. A Hairpin target Dynamic is emitted at the same
endpoint, including an endpoint represented as beat zero immediately after the final measure.
The compiler computes one divisions LCM over every emitted rational timing value: event durations
and offsets, global point positions, span boundaries, and global or measure-local measure
durations. Its tractable implementation profile requires both that LCM and every resulting
non-negative duration-unit value to fit `int`; an unrepresentable valid Score returns
`ArithmeticOverflow` before a partial document is emitted. Thus a denominator used only by a
mid-measure tempo, key, rehearsal, Direction, Hairpin, or PartDirective cannot be truncated merely
because the notes use a coarser grid.
Clef and barline nodes do not carry offsets; for
those point variants the compiler uses positive `backup`/`forward` pairs to visit the exact point
and restore the Voice cursor. This cursor restoration is required for subsequent notes and for the
multi-Voice backup invariant.

ChordSymbol root, bass, registered quality, inversion, and degree operations use structured
MusicXML fields. A structured numeral selects MusicXML's mutually exclusive `numeral` branch in
place of `root`: it emits the one-based numeral root, optional integer alteration, and required
`numeral-key` fifths/mode. Optional `roman` becomes only the numeral-root display spelling; it does
not replace the typed semantics. Ordered degrees emit required value, alteration, and
add/alter/subtract type children. A structured symbol without free-form extensions therefore
produces no harmony-loss diagnostic.

Without `numeral`, legacy `roman` remains visible together with `extensions` in `kind` display text
and produces a compilation diagnostic. Free-form extensions likewise remain a diagnosed display
residual even when a numeral exists; callers use `degrees` for semantic target alignment. Unknown
qualities retain their original spelling as display text with MusicXML kind `other`.

The validated `KeySignature.accidentals` field is authoritative for MusicXML `fifths`; the
compiler does not independently reinterpret it from the tonic. S21 has already proved coherence
for standard modes, while custom scales deliberately retain their stored fifths. MusicXML 4's native `major`, `minor`,
`ionian`, `dorian`, `phrygian`, `lydian`, `mixolydian`, `aeolian`, and `locrian` mode values retain
those Sunny scale names. Other scale definitions retain the exact fifths count, emit mode `none`,
and produce a diagnostic. MusicXML's non-traditional alternative is not emitted: it requires an
explicit ordered set of altered diatonic steps and alterations, while Sunny's current custom-scale
payload contains chromatic offsets plus a separately authoritative traditional fifths count and
cannot supply those spellings without invention. A global key entry inside a measure is positioned with an exact
`forward`/`attributes`/`backup` cursor visit; it is not pulled back to the downbeat. A one-measure
local key or time override is compared against the next measure's effective global state, so the
compiler emits the required reset. Unequal `TimeSignature.groups` are serialized as an additive
MusicXML numerator (for example, groups `[3,2]` become `3+2`), while equal simple/compound groups
retain the conventional summed numerator.

### 9.7 LilyPondCompiler

**Definition 9.7.1**. The *LilyPondCompiler* produces LilyPond input files (.ly) for engraving-quality PDF score generation.

The compilation maps the Score IR hierarchy to LilyPond 2.24 textual music notation. For the
properties covered below, the target boundary has three dispositions: a lossless native construct;
a visible textual construct plus a compilation diagnostic when LilyPond lacks the required
semantic algebra; or rejection by the shared structural preflight. Target-external PartDirective
semantics remain governed by §3.3 rather than being implicitly claimed as engraving constructs.
The same boundary applies to tuning and release velocity: the admitted profile preserves notation
pitch but not a complete 128-note sounding-pitch function or Note Off intensity. Non-standard
tuning is a counted compilation residual, and every non-default release velocity is a
location-aware diagnostic; neither is silently discarded or translated into an ad hoc Scheme
fragment. Coherent `Note.dynamic` and `VelocityValue.written` state both produce the corresponding
dynamic command. An active numeric-only attack velocity produces a location-aware diagnostic
because the admitted profile has no exact per-note MIDI-byte construct. The compact `NoteEvent`
fragment adapter rejects a non-default release value because it cannot return residual evidence.
The document-level `SectionMap`, persisted harmonic and orchestration analysis, optional governing
tone row, and both stale-analysis region sets likewise produce explicit diagnostics when populated.
Voice `ChordSymbol` events remain distinct from the global harmonic-analysis layer, and rendering
`dynamic_balance` is not reinterpreted as an engraving dynamic.

| Score IR property | LilyPond projection |
|---|---|
| Arbitrary positive `Beat` | Notes and visible rests use the longest writable tied values (an inferred `\tuplet` when the duration implies one); a duration with no written form keeps an exact `*N/M` multiplier and is reported. Spacers and whole-measure rests keep exact multipliers |
| `RestEvent.visible=false` | Spacer rest `s`, never printed `r`/`R` |
| Hierarchical SectionMap and document analysis layers | Location-aware compilation residuals; no partial engraving flattening is claimed |
| Per-note chord tie/articulation/fingering/string/notehead | Attachment inside the `< … >` chord construct |
| Dynamic / sforzando / forzando-piano | Chord-level dynamic post-event; conflicting per-note ownership is diagnosed; a Hairpin target is emitted at its exact endpoint |
| Grace NoteGroup | Whole note or chord in `\acciaccatura` / `\appoggiatura`, followed by an exact spacer for Sunny's structural allocation |
| Native NoteHead | Per-note `\tweak style`; `Cue` is a sounding `font-size #-2` note |
| Square NoteHead | Ordinary sounding head plus visible `square notehead` marker and diagnostic because 2.24 has no standard square style |
| Ornament | Native trill/mordent/turn/reverse-turn/shake/arpeggio where the model is sufficient |
| TechnicalDirection | Native fingering, string number, rational-cent `\bendAfter`, breath, or caesura; otherwise visible note text plus diagnostic |
| LyricSyllable | One named `NullVoice` alignment lane plus native `Lyrics \lyricsto` context per verse; `--`, `_`, and `__` project word, skip, and melisma state exactly |
| BeamGroup | Exact explicit primary `[` / `]` manual beam; S14 rejects secondary-break state that would require per-stem left/right beam counts |
| Nested TupletContext | Nested `\tuplet actual/normal { ... }` blocks; notes, rests and spacers use their sounding allocation multiplied by every enclosing `actual/normal` ratio |
| PartDirective | Exact-time italic start text and explicit `end …` text in the Part's annotation layer |
| ChordSymbol | Exact-time bold display text retaining root, quality, bass, Roman text, and extensions; structured numeral/key/inversion/degree state remains source-owned and produces a chordmode-semantic residual |
| TempoMap | Exact stored beat unit and rational rate; a Linear label is placed at the previous event and its exact target mark at the destination; metric units remain visible; target curve limits are diagnosed |
| KeySignature | Native major/minor and seven church-mode commands; other scales set the exact stored traditional signature through `Staff.keyAlterations` and diagnose the target-external analytical tonic/scale |
| Unequal TimeSignature groups | `\compoundMeter` with the stored group sequence |
| Part staff topology | One `PianoStaff` containing ordered `Staff` contexts with their effective initial clefs |
| Transposition | `\transpose` whose pitch is spelled from the diatonic written-to-sounding interval |
| Voice staff ownership | Voice music and point annotations are emitted only in the owning `Staff`; a staff with no Voice receives an exact spacer timeline |

LilyPond note/chord music and zero-duration metadata use separate synchronized layers. The measured
voice contains only NoteGroups and Rests. A parallel spacer sequence, joined to the first voice
with `<< { … } { … } >>` and no `\\` separator, visits the sorted union of
Direction, ChordSymbol, TempoMap, RehearsalMark, KeySignature, and Hairpin boundary offsets, emits
the target command at that exact point, and fills the remaining measure duration with exact
spacers. This preserves a direction inside a sustained note/rest instead of moving it to the
span's end. The separator would create new voices with forced stems and break ties at the
barline, so only genuine polyphony uses it. Global tempo/rehearsal events appear only on the first Staff; Part hairpins and
voice-owned point events appear once in their owning Staff.

LilyPond permits partial chord ties and per-note articulations inside a chord but requires dynamics
and hairpins at chord/voice scope. The compiler follows that ownership rule. All programmed
`ArticulationType` variants have exactly one compile-time-checked disposition. First-class Ornament
metadata supersedes a matching legacy ornament articulation, while richer trill interval or
accidental state that a simple LilyPond script cannot retain produces explicit evidence.

---

## 10. Validation

### 10.1 Validation Model

The Score IR maintains a set of invariants checked by explicit validation. Construction workflows
validate their result; mutation operations enforce their own transactional preconditions and mark
affected analysis layers stale; callers use `validate_score` for a complete diagnostic view.
Compilers apply structural preflight plus the domain rules owned by their target. The historical
`is_compilable` name is the target-independent structural predicate, not an assertion that every
musical or rendering diagnostic is absent.

Validation produces a list of diagnostics, each classified by severity:

| Severity | Meaning | Effect |
|----------|---------|--------|
| `Error` | Invariant or target-domain violation | Blocks its owning validator/target policy; an explicitly degradable musical event may instead be dropped with typed compilation evidence |
| `Warning` | Potential problem; document is valid but may not render as intended | Compilation proceeds; diagnostic reported |
| `Info` | Observation for the agent's consideration | No effect on compilation |

### 10.2 Structural Validation Rules

| Rule | Severity | Description |
|------|----------|-------------|
| S0/S0b | Error | A score has at least one Part and every Measure has at least one Voice |
| S1 | Error | Every part has exactly `total_bars` measures |
| S2 | Error | Positive-duration NoteGroup/Rest events tile `[0, measure_duration)` exactly; every event offset is in range and no measured span crosses the boundary |
| S3 | Error | Events are ordered by offset and positive-duration NoteGroup/Rest spans do not overlap; point events may occur within a measured span |
| S4 | Error | TempoMap begins exactly at `ScoreTime(1, 0)`; every entry is an in-score, in-meter point; entries are strictly ordered |
| S5 | Error | TimeSignatureMap begins at bar 1 and its entries are in-score and ordered; group positivity/numerator representability/power-of-two denominator are `TimeSignature` construction and parser invariants, so validation emits no diagnostic for an impossible malformed value |
| S6 | Error | KeySignatureMap begins exactly at `ScoreTime(1, 0)`; every entry is an in-score, in-meter point; entries are strictly ordered |
| S7 | Error | A tied note's next measured event in the same voice is a NoteGroup containing the same ordinary (non-grace) pitch; intervening point events do not break adjacency |
| S8 | Error | Tuplet definitions agree; ratios are positive; nesting is acyclic; descendant measured members are contiguous and their exact durations sum to the ancestor-scaled structural span; `actual` counts rhythmic units rather than events |
| S9 | Error | Section spans do not overlap at the same nesting level |
| S10 | Error | Section hierarchy is properly nested |
| S11 | Error | Tone row (if present) is a valid permutation of 12 pitch classes |
| S12 | Type invariant | Reserved: every `Beat` is canonical with a positive denominator by construction; JSON readers reject malformed ratios before a Score exists, so validation emits no S12 diagnostic |
| S13 | Error | Root Score identity is assigned (nonzero), and Event, Part, and Section identifiers are unique in their document scope. Authoring operations choose the lowest unused positive component value in the candidate document; imported high values do not force wrap or collision. |
| S14 | Error/Warning | BeamGroup IDs, membership, order, contiguity, written-duration eligibility, and NoteGroup back-references satisfy the explicit-primary-beam profile; Tuplet references are valid; stale NonChordTone analysis references are advisory |
| S15 | Error | Harmonic annotations are ordered and non-overlapping; every entry has a positive exact span wholly inside the score and a closed, complete, ascending, inversion-coherent chord-analysis payload (§6.2) |
| S16 | Error | Orchestration annotations have existing Part ownership, valid non-overlapping in-score spans per Part, closed role values, and exactly the required valid role-specific references/payloads |
| S17 | Error | A NoteGroup does not mix ordinary/grace notes or grace types, and a grace note has no duration tie |
| S18 | Error | Measures use canonical 1-based vector-order bar numbers and Voices have unique strictly increasing indices |
| S19 | Error/Warning | Direction variants contain their required payload (`text`, `new_clef`, or a supported ottava shift); irrelevant populated payload fields are advisory |
| S20 | Error/Warning | Hairpin and PartDirective spans are non-empty and within their Part; hairpins do not overlap; enum payloads are in range; Divisi requires a count of at least two and irrelevant counts are advisory |
| S21 | Error | Every scale payload has a canonical ordered pitch-class profile; any registry-resolving name exactly matches its registered definition; standard major/minor/church-mode keys—including HarmonicAnnotation key contexts—have coherent tonic/mode/fifths identity, while other scales retain their independent stored count |
| S22 | Error | `staff_count` is positive; per-staff clefs are empty or complete and in-domain; every Voice staff assignment is within its owning Part |
| S23 | Error | Slur/glissando endpoints form one non-overlapping paired span per Voice; ottava points form coherent Staff state; pedal points form coherent Part state and agree with any duplicate SustainingPedal directive; ottavas close and a terminal open pedal explicitly continues through the final bar line |
| S24 | Error | `PositiveRational` rate validity is a type/parser invariant; tempo enum payloads are valid; the first transition is Immediate; Linear duration equals its complete preceding interval; MetricModulation beat units and exact derived effective-quarter rate agree |
| S25 | Error | NoteGroup, Note, ChordSymbol, Ornament, TechnicalDirection, and their nested enum/pitch/velocity payloads are in their closed structural domains; a structured chord numeral's explicit key/degree/alteration must derive the exact same root letter and accidental as the stored ChordSymbol root |
| S26 | Error | Lyrics are first-note onset metadata with canonical unique verses; every verse lane has closed word chains and each melisma spans a later ordinary NoteGroup |
| S27 | Error | Every Section node has a nonempty label, a non-empty in-score half-open span, and an in-domain optional FormFunction; empty or partial SectionMaps remain valid |
| S28 | Error | ScoreTuning has an in-domain reference index, finite positive reference frequency, exactly 128 finite cent positions, zero at the reference index, and a finite positive derived frequency for every note |

### 10.3 Musical Validation Rules

| Rule | Severity | Description |
|------|----------|-------------|
| M1 | Warning | Note outside instrument's comfortable range |
| M2 | Error | Note outside instrument's absolute range |
| M3 | Warning | Parallel fifths or octaves between outer voices (configurable) |
| M4 | Warning | Voice crossing between adjacent voices |
| M5 | Warning | Leap larger than an octave without stepwise resolution |
| M6 | Info | Unresolved leading tone in V → I progression |
| M7 | Info | Unresolved chordal seventh |
| M8 | Warning | Harmonic annotation layer is stale (inconsistent with note content) |
| M9 | Info | Missing orchestration annotation for a non-tacet region |
| M10 | Warning | Dynamic marking absent for more than 16 bars in a non-tacet part |

### 10.4 Rendering Validation Rules

| Rule | Severity | Description |
|------|----------|-------------|
| R1 | Warning | Part has no RenderingConfig.instrument_preset (the compiled track remains without an explicitly requested instrument) |
| R2 | Warning | Articulation used but not present in part's articulation_vocabulary |
| R3 | Warning | Articulation used but no ArticulationMapping defined for it |
| R4 | Info | Grace note duration exceeds half the following note's duration |
| R5 | Warning | TickTime rounding error exceeds 0.5 ticks for a bar |
| R6 | Error | Rendering channel, expression CC, pan, or articulation mapping is outside its domain |
| R7 | Error | Explicit numeric attack or release velocity exceeds 127, or `Note.dynamic` conflicts with `VelocityValue.written` |

---

## 11. Mutation Operations

### 11.1 Mutation Model

The Score IR supports mutations at every level of the hierarchy. Mutations are the primary
interface through which an agent or user modifies a valid score; they are operations on the model,
not raw field assignments. A successful mutation increments the document version, marks the
applicable analysis layers stale, and, when an `UndoStack` is supplied, records the exact prior
document snapshot. A rejected mutation leaves the document, version, and undo/redo stacks
unchanged.

Topology-changing operations construct or prove the complete affected measure partition before
commit. Semantic endpoint deletion cascades to its paired endpoint. A transform that cannot retain
a Voice-scoped span or BeamGroup identity removes the complete relation and returns a `MUT1`
warning; it never leaves an orphan or silently assigns a new identity. Typed-ID allocation is
document-local rather than process-global: each candidate operation indexes both represented IDs
and `Score.identity_reservations`, assigning the lowest unused positive value in its own typed
domain. Event, Part, Section, Tuplet, and BeamGroup reservations are separate ordered sets. Each
successful raw mutation retains the union of its before/after exposed IDs, including objects
retired by deletion or notation repair. Failure discards private allocation work and leaves the
reservations, document, version, and history unchanged. Reserving an imported `UINT64_MAX` reserves
only that value; it does not reserve untouched holes below it or force a max-plus-one cursor. No
allocator performs a wrapping increment.

These reservations belong to the Score value and its canonical codec, so private candidates,
immutable snapshots, and save/load cycles retain the same identity history without process-global
or thread-local state. Directly assembled Score values remain admissible without manually seeding
the sets: allocators also inspect actual represented IDs, and successful mutation/ownership
boundaries and serialization collect them. The public `collect_score_identities` helper collects
represented plus reserved identities; `retain_score_identities(restored,current)` unions both
documents' identity histories without remapping content or changing their versions.

Pure reduction views that replace all event content number their new events deterministically
from one under their caller-selected new root identity; part extraction indexes retained events
before adding cues. Reduction APIs propagate failure through `Result<Score>`.

The version domain is the closed `uint64` interval. Before any raw mutation that would publish a
new Score state, the implementation admits the operation only if `version < UINT64_MAX`. An
exhausted version returns `ArithmeticOverflow` before changing the Score, stale-region state, or
either side of an attached `UndoStack`; it never wraps to zero. Lower-level candidate validation
also uses a checked increment, so version safety is not dependent on unchecked integer arithmetic.
For a raw mutation entry point, version admission is the first check and `ArithmeticOverflow`
therefore takes precedence over operation-specific argument errors. Undo and redo first determine
whether their requested history direction exists because an unavailable traversal is not a state
transition; only an available traversal then applies the exhaustion check.

### 11.2 Event-Level Mutations

| Operation | Parameters | Effect |
|-----------|-----------|--------|
| `InsertNote` | part, bar, voice, offset, note, duration | Replaces exact rest coverage, or merges an exact coincident ordinary chord; partial musical overlap is rejected and an intersected BeamGroup is removed with `MUT1`. A unit tuplet duration `1/(2^a m)` with odd `m > 1` that lands in plain untupleted silence first divides its aligned tuplet span (`m` units, within the bar) into unit rests under a fresh `m : n` TupletContext, `n` the largest power of two below `m`; the note then replaces its unit, so three successive 1/12 notes form a complete 3:2 tuplet of eighths |
| `DeleteEvent` | event_id | Replaces measured content with an equal rest or removes a point; paired notation endpoints cascade |
| `ModifyPitch` | event_id, note_index, new_pitch | Changes the pitch of a note within a NoteGroup |
| `ModifyDuration` | event_id, new_duration | Changes a fixed-onset measured duration; growth consumes only rests, shrinkage creates rests, and collisions reject atomically |
| `ModifyVelocity` | event_id, note_index, new_velocity | Changes the velocity |
| `ModifyReleaseVelocity` | event_id, note_index, new_release_velocity | Changes Note Off velocity in `[0,127]` |
| `SetArticulation` | event_id, note_index, articulation | Adds or changes an articulation |
| `SetDynamic` | part_id, position, dynamic_level | Inserts a dynamic marking |
| `InsertHairpin` | part_id, start, end, type, target | Inserts a crescendo/diminuendo |
| `SetTie` | event_id, note_index, tied | Sets or clears a tie forward |
| `TransposeEvent` | event_id, interval | Transposes all notes by a diatonic interval |

### 11.3 Measure-Level Mutations

| Operation | Parameters | Effect |
|-----------|-----------|--------|
| `InsertMeasure` | after_bar, count | Inserts positive-count empty measures after an existing bar; shifts all nested points/spans and recomputes exact incoming ramp durations |
| `DeleteMeasure` | bar, count | Removes a proper subset of measures; clips/collapses crossing spans, repairs endpoints, and materialises exact splice state for global maps |
| `SetTimeSignature` | bar, time_signature | Retiles rest coverage until the next meter entry and recomputes incoming ramp durations; clipping musical/point content rejects atomically |
| `SetKeySignature` | position, key_signature | Updates an in-score/in-meter key point and rejects an invalid resulting key identity |
| `AddVoice` | part, bar | Adds a new voice to a measure |
| `RemoveVoice` | part, bar, voice_index | Removes a non-final Voice and repairs any notation span crossing its boundary |

### 11.4 Part-Level Mutations

| Operation | Parameters | Effect |
|-----------|-----------|--------|
| `AddPart` | definition, position_in_order | Adds a new part with empty measures |
| `RemovePart` | part_id | Removes a part and all its content |
| `ReorderParts` | new_order | Changes the ordering of parts in the score |
| `SetPartDirective` | part_id, start, end, directive | Adds a performance directive |
| `AssignInstrument` | part_id, instrument_preset | Updates the RenderingConfig |

### 11.5 Region-Level Mutations

| Operation | Parameters | Effect |
|-----------|-----------|--------|
| `TransposeRegion` | region, interval | Transposes all notes in the region by a diatonic interval |
| `CopyRegion` | source_region, target_position | Copies NoteGroups into exact rest coverage; complete slur/glissando pairs survive, boundary-crossing endpoints are stripped, and source-local BeamGroup references are removed with `MUT1` |
| `MoveRegion` | source_region, target_position | Moves note content (copy + delete source), removing source/destination beam identity with `MUT1` |
| `DeleteRegion` | region | Replaces all content in the region with rests |
| `SetDynamicRegion` | region, dynamic_level | Applies a dynamic to all parts in the region |
| `ScaleVelocityRegion` | region, factor | Multiplies all velocities in the region by a factor |
| `RetrogradeRegion` | region | Reverses measured payloads within each affected measure, repacks unequal durations, keeps point times, and removes touched slur/glissando spans and BeamGroups with `MUT1` |
| `InvertRegion` | region, axis_pitch | Inverts all pitches about a given axis |
| `AugmentRegion` | region, factor | Multiplies fixed-onset non-tuplet NoteGroup durations; rest coverage is reconstructed, musical collisions reject atomically, and touched BeamGroups are removed for re-beaming with `MUT1` |
| `DiminuteRegion` | region, factor | Divides fixed-onset non-tuplet NoteGroup durations, materialises the resulting rest coverage, and removes touched BeamGroups for re-beaming with `MUT1` |

### 11.6 Orchestration Mutations

| Operation | Parameters | Effect |
|-----------|-----------|--------|
| `Reorchestrate` | source_part, target_part, region | Copies melodic content to the target Part; Voice-scoped spans and BeamGroup references are stripped with `MUT1` because their identity is not cross-Part |
| `DoubleAtInterval` | source_part, target_part, region, diatonic interval | Copies the melody to the target Part, spelling each copy by applying the diatonic interval (D4 up a perfect fifth is A4); a result outside the SpelledPitch domain rejects the mutation. Each source tuplet's member layout is first laid as rests under a fresh tuplet identity over the target's silence, so tuplet passages double; nested tuplets are refused. The same `MUT1` notation-identity rule applies |
| `SetTextureRole` | part_id, region, role | Updates the orchestration annotation for a part in a region |
| `ApplyVoiceLeading` | region, constraints | Runs the Theory Spec's voice-leading algorithm on the harmonic content in the region and distributes voices to parts according to their roles |

### 11.7 Undo/Redo

Mutations record the complete preceding document snapshot when an `UndoStack` is supplied (§11.1). Undo saves the live document onto the redo stack and restores the popped content; redo mirrors this. Restored content keeps its original typed IDs, while its reservations union the restored and current histories. Thus inserting Event 1, undoing it, and inserting different content allocates Event 2 rather than reusing Event 1. Full-content identity comparisons exclude the monotonic version and reservation metadata. Any new mutation clears redo history; reservation history survives capacity pruning and disabled undo recording.

**Group operations**: A sequence of mutations can be grouped as a single undoable unit (e.g., "reorchestrate bars 33–48" might involve dozens of individual mutations, but undoing it reverts all of them at once).

**Version counter under undo/redo**: Undo and redo are themselves mutations: they modify the document state and therefore *increment* the version counter. The version counter records the total number of state transitions, not the logical edit distance from the initial document. This means:

- After edit A (version 1 → 2), undo (version 2 → 3), redo (version 3 → 4): the version is 4, not 2.
- The version counter is strictly monotonically increasing and is never reused, even across undo/redo cycles.
- Two documents with the same version number are guaranteed to represent the same state only if they share a common lineage (same document identity). The version counter is a Lamport timestamp: it provides a total order on state transitions but not a content hash.
- If the live version is `UINT64_MAX`, an otherwise available undo or redo returns
  `ArithmeticOverflow` before popping, pushing, or replacing either the Score or history state.

**Undo stack capacity**: Every entry is a full Score copy, so the history is bounded. An `UndoStack` retains at most `capacity` undo snapshots (64 by default; zero disables history) and discards the oldest when a new mutation would exceed it. Redo entries are created only by undo, so they never exceed the capacity either. The MCP `score_undo` and `score_redo` tools expose this history per score.

---

## 12. Agent Interface

### 12.1 Query Operations

The Score IR exposes a query interface for agents to read the document without mutation. Queries return structured data, not raw score fragments, so that the agent receives information at the appropriate abstraction level.

| Query | Parameters | Returns |
|-------|-----------|---------|
| `GetHarmonyAt` | position | HarmonicAnnotation at that time |
| `GetHarmonyRange` | start, end | Sequence of HarmonicAnnotations |
| `GetMelodyFor` | part_id, region | Sequence of (pitch, duration, offset) tuples |
| `GetChordProgression` | region | Sequence of ChordSymbols with timing |
| `GetOrchestration` | region | Map from part_id to TexturalRole |
| `GetPartsWithRole` | role, region | List of part_ids carrying that role |
| `GetActiveParts` | position | List of part_ids that are not tacet |
| `GetPitchContent` | region | Combined pitch class set across all parts |
| `GetRange` | part_id | Actual sounding range (lowest and highest pitch) in the part |
| `GetDynamicAt` | part_id, position | Current effective dynamic level |
| `GetTempoAt` | position | Current BPM and beat unit |
| `GetKeyAt` | position | Current key signature |
| `GetTimeSignatureAt` | bar | Current time signature |
| `GetSectionAt` | position | Section label and formal function |
| `GetSections` | — | Full section hierarchy |
| `GetFormSummary` | — | Sequence of (section_label, start, end, key, tempo) |
| `FindMotif` | pitch_pattern, region | List of occurrences |
| `GetReduction` | view_type, region | Reduced view of the specified type |
| `Validate` | region (optional) | List of diagnostics |

### 12.2 MCP Tool Integration

The Score IR exposes its operations as MCP (Model Context Protocol) tools, extending the existing Sunny MCP server. The tools are organised by abstraction level:

**Composition tools** (high-level, operating on formal structure):

| Tool | Description |
|------|-------------|
| `score_create` | Initialise a new Score IR with metadata, parts, tempo, key, time signature, optional complete tuning (otherwise canonical 12-TET/A4=440), and a root identity equal to the returned repository handle |
| `score_set_tuning` | Atomically replace the name, reference note/frequency, and exact 128-entry cent function |
| `score_set_formal_plan` | Define the SectionMap (e.g., "sonata form in D major, 3 sections") |
| `score_add_part` | Add an instrument to the score with Appendix A defaults and the first free MIDI channel |
| `score_remove_part` | Remove a Part and its owned bound siblings; refuse retained incoming references |
| `score_reorder_parts` | Apply an exact Part permutation while retaining typed identities |
| `score_insert_measures` | Insert whole bars and atomically relocate bound automation and morph anchors |
| `score_delete_measures` | Delete whole bars, report removed controls, clip morph spans, and relocate survivors |
| `score_set_time_signature` | Apply positive ordered additive groups and a power-of-two denominator; bound controls must still fit |
| `score_set_key_signature` | Apply an exact spelled root and registered mode; derive or validate the fifths count |
| `score_set_tempo_map` | Replace a caller-ordered exact rational map with explicit incoming linear/metric payloads |
| `score_assign_instrument` | Apply standard notation range/transposition while preserving concert pitches and custom rendering/staff settings; refuse pitched/unpitched migration |
| `score_set_section_harmony` | Atomically construct complete registered chord voicings and per-position local-key analyses for a section; reject out-of-range members, unknown qualities, and non-chord slash basses |

**Arrangement tools** (mid-level, operating on parts and regions):

| Tool | Description |
|------|-------------|
| `score_write_melody` | Write a melodic line into a part for a region |
| `score_write_harmony` | Write chord voicings into one or more parts |
| `score_reorchestrate` | Move musical material between parts |
| `score_double_part` | Create a doubling of one part in another at a semitone interval with an optional diatonic step count (default: the conventional perfect, major, or minor interval, with the tritone as an augmented fourth) |
| `score_set_dynamics` | Apply dynamic markings to a region |
| `score_set_articulation` | Apply articulations to notes in a region |
| `score_set_articulation_mapping` | Set or remove one validated part/articulation MIDI mapping |
| `score_delete_region` | Delete a half-open exact ScoreTime span |
| `score_copy_region` | Copy an exact span to an exact destination |
| `score_move_region` | Move an exact span to an exact destination |
| `score_retrograde_region` | Reverse allocations within an exact span |
| `score_invert_region` | Reflect pitches around an exact spelled axis |
| `score_augment_region` | Multiply durations/offsets by an exact positive factor |
| `score_diminish_region` | Divide durations/offsets by an exact positive factor |

**Detail tools** (low-level, operating on individual events):

| Tool | Description |
|------|-------------|
| `score_insert_note` | Insert a bounded note by carving covered rests, or merge it into an exactly coincident chord; reject partial musical overlap |
| `score_insert_chord_symbol` | Insert one zero-duration typed harmony event; validate numeral/root coherence atomically and preserve undo identity |
| `score_modify_note` | Change pitch, duration, attack velocity, release velocity, or articulation |
| `score_delete_event` | Remove an event |
| `score_set_tie` | Set or clear the forward tie of one note; a tie needs an adjacent same-pitch continuation, so both notes are inserted first |
| `score_add_voice` | Add a distinct voice in one Part/bar, optionally selecting a staff |
| `score_remove_voice` | Remove one structural voice, retaining its observed event identities |
| `score_insert_hairpin` | Add an exact crescendo/diminuendo span and optional target dynamic |
| `score_create_tuplet_group` | Attach a new standalone context to an exact contiguous measured span whose already-scaled allocation equals `normal * normal_type`; nesting/conflicts are refused |
| `score_remove_tuplet_group` | Remove standalone notation without changing note/rest allocations; nested references are refused and retired IDs remain reserved |
| `score_transpose` | Transpose a note, event, or region |

**History tools**:

| Tool | Description |
|------|-------------|
| `score_undo` | Restore the document state before the most recent mutation of that score |
| `score_redo` | Reapply the most recently undone mutation; any new mutation clears redo history |

**Analysis tools** (read-only):

| Tool | Description |
|------|-------------|
| `score_analyze_harmony` | Get harmonic analysis for a region |
| `score_get_orchestration` | Get textural role assignments |
| `score_get_reduction` | Get a piano, short-score, or harmonic-skeleton reduction under a newly allocated root identity; reject unknown views/regions |
| `score_validate` | Run validation and return diagnostics |
| `score_get_form_summary` | Get the formal structure overview |
| `score_get_json` | Serialise the complete Score IR document |

**Query tools**:

| Tool | Description |
|------|-------------|
| `score_query_harmony_at` | Find the harmonic annotation active at an exact score time |
| `score_find_motif` | Find occurrences of a pitch-class motif within a region |

**Compilation tools**:

| Tool | Description |
|------|-------------|
| `score_compile_to_midi` | Compile to MIDI event data |
| `score_export_midi` | Serialize actual SMF type-0 bytes as base64, preserving the compilation report |
| `score_compile_to_musicxml` | Compile to MusicXML |
| `score_compile_to_lilypond` | Compile to LilyPond |

`score_create` always constructs a complete standard key identity. Its `minor` flag selects the
registered major or minor scale definition. When `key_accidentals` is omitted, the signed fifths
count is derived from the tonic spelling and selected mode; a supplied contradictory count is
rejected by S21 rather than being forwarded to different target interpretations.

`score_create` requires at least one part, as rule S0 does. Its `bpm` is stored as the exact
rational of the shortest decimal that round-trips the supplied number, so 92.5 BPM is 185/2 rather
than a truncated 92. Every part created through `score_create` or `score_add_part` takes its
sounding range, transposition, and default clef from Appendix A (a caller-supplied clef is kept);
an instrument that Appendix A does not list receives the full MIDI range C-1–G9, no transposition,
and the treble clef, so the range constrains nothing rather than inventing a limit. Each new part
also receives a distinct default MIDI channel: unpitched percussion takes General MIDI channel 10,
and any other instrument takes the lowest channel in 1–16 other than 10 that no existing part
occupies, sharing the least-occupied channel (lowest number on ties) once all fifteen are taken.

Every MCP region names whole bars with an inclusive `end_bar`: `{start_bar: 1, end_bar: 1}` is bar 1
alone. The tools convert it to the half-open core `ScoreRegion` ending at the downbeat after
`end_bar`.

The Score tools do not write to Ableton. Live deployment of a Score, alone or with its Timbre and
Mix documents, goes through the guarded Project tools (`project_validate`,
`project_plan_to_ableton`, `project_apply_ableton_plan`, `project_compile_to_ableton`), which plan
against a captured target snapshot and journal every mutation (`sunny-project-model.md`).

### 12.3 Agent Workflow Patterns

The following are reference workflow patterns for different compositional tasks.

**Solo piano piece**:

1. `score_create` with one Piano part.
2. `score_set_formal_plan` (e.g., ABA ternary).
3. For each section: `score_set_section_harmony` → `score_write_melody` → `score_write_harmony` (inner voices).
4. `score_set_dynamics` and `score_set_articulation` for expression.
5. `score_validate`.
6. `project_compile_to_ableton`.

**Orchestral work**:

1. `score_create` with full orchestral parts (strings, woodwinds, brass, percussion).
2. `score_set_formal_plan` (sonata form: Exposition, Development, Recapitulation, Coda).
3. Compose the harmonic skeleton: `score_set_section_harmony` for each section.
4. Compose primary thematic material: `score_write_melody` into a lead part (e.g., Violin I).
5. Orchestrate: `score_reorchestrate` and `score_double_part` to distribute material across the ensemble.
6. Add countermelodies: `score_write_melody` into secondary parts.
7. Fill harmonic support: `score_write_harmony` into sustaining instruments.
8. Add rhythmic material: `score_write_melody` for ostinato or rhythmic patterns.
9. Apply dynamics and articulations: `score_set_dynamics`, `score_set_articulation`.
10. Review via `score_get_reduction` (piano reduction) for harmonic coherence.
11. `score_validate` — resolve any range, doubling, or voice-leading issues.
12. `project_compile_to_ableton`.
13. Listen, revise at the Score IR level, re-compile.

**Electronic / production track**:

1. `score_create` with synthesiser and drum machine parts.
2. `score_set_formal_plan` (Intro, Verse, Chorus, Bridge, Drop, Outro).
3. Use `apply_euclidean_rhythm` (existing MCP tool) for rhythmic patterns.
4. `score_write_melody` for synth leads.
5. `score_set_section_harmony` for harmonic content.
6. `project_compile_to_ableton` — the DAW becomes the primary editing environment for sound design, while the Score IR manages structure and arrangement.

---

## 13. Serialisation

### 13.1 On-Disk Format

The implemented Score IR format is a single versioned JSON document for human readability and tool interoperability.

**JSON schema**: Follows the type definitions in this specification directly. Field names are snake_case. Enumerations use their checked integer representation. Beat values are serialised as `{"n": numerator, "d": denominator}`. SpelledPitch is serialised as `{"letter": 0, "accidental": 0, "octave": 4}`, where `letter` is in `[0, 6]`. Typed IDs are unsigned JSON integers.

**Reserved binary format**: A canonical compact encoding may be added for very large scores, but no `SNSC`/TLV binary codec is implemented or advertised by the current runtime. Clients must use JSON.

### 13.2 Versioning

The serialisation format includes a schema version number. The current library writes schema
version 9 and accepts versions 1 through 9, applying its version-specific defaults while loading
older documents. Schema versions 1–3 used one optional unstructured `lyric` string; loading
migrates it to verse 1, `Single`, without an extender. Version 4 writes the structured `lyrics`
array and persists the owned key-mode `name` and non-semantic `description`; an older key payload
that lacks those strings migrates as an anonymous scale while retaining its interval profile.
Versions 1–4 may contain an integer `state` field whose `Draft`, `Valid`, `Compiled`, and `Locked`
labels were never enforced and could become stale after any edit. Version 5 range-checks that
legacy field while loading and discards it; current output omits it, and a v5 input containing it
is rejected rather than accepted as false lifecycle authority.
Version 6 adds the required `tuning` object with exactly 128 cent entries. Versions 1–5 without
that object migrate to the canonical 12-TET/A4=440 pitch function; a legacy-labelled document that
already contains the complete object preserves it rather than silently discarding source intent.
Version 7 requires every Note to carry integer `release_velocity` in `[0,127]`; versions 1–6
migrate its historical absence to the neutral value 64. A legacy-labelled document that already
contains the field preserves and validates it rather than discarding forward source evidence. A
current document may not omit it. Version 8 adds optional structured `numeral`, `inversion`, and
`degrees` fields to ChordSymbol. Versions 1–7 migrate their absence to empty option/vector values;
a legacy-labelled document already carrying them preserves and validates that forward evidence.
When present, every nested numeral key and degree field is mandatory.
Version 9 requires `identity_reservations` with five arrays named `events`, `parts`, `sections`,
`tuplets`, and `beams`. Each is a strictly increasing, duplicate-free sequence of checked unsigned
64-bit IDs; together they must cover the document's actually represented IDs in those domains.
Writers collect actual IDs as well as stored retired reservations, giving directly assembled values
one canonical encoding. Versions 1–8 without this metadata migrate by reserving only IDs actually
represented in the loaded document: unavailable historical deletions cannot be reconstructed, and
no ranges beneath an imported maximum are invented. A legacy-labelled file carrying explicit
reservation metadata preserves and validates it, then unions actual IDs.
Missing, zero, or future versions are rejected explicitly; clients must not assume that an
older runtime can read a newer document.

### 13.3 Invariant Re-Validation on Load

When a Score IR document is deserialised, full validation (§10) runs before the document is made
available. Structural (`S*`) and rendering-domain (`R*`) errors reject the document. Musical
diagnostics remain available to target policies that explicitly degrade individual events with
typed evidence. Tagged articulation mappings are additionally validated while parsing, before
they can enter the document graph.

---

## 14. Document Lifecycle

### 14.1 State Machine

A Score has no persisted lifecycle-state enumeration. Construction, validation, mutation,
compilation, and serialisation are operations over a value, not stable mutually exclusive states:
a caller may serialise an immutable snapshot while another transaction prepares a later candidate,
and compilation success for one target says nothing about a later target or version.

`create_score` and `score_from_json` construct private candidates and return no Score until their
required validation succeeds. A raw Score is usable for the duration of its C++ value lifetime and
requires exclusive ownership for mutation. A `ScoreDocument` is usable while at least one handle
exists; retained immutable snapshots have independent shared lifetime. Destruction of the last
owner is the only close operation. There is no explicit `close()` method and no valid reference is
invalidated by a hidden state transition.

Validity is derived by the validation functions for the exact version examined. Compilation is a
result with target-specific evidence for that exact input version. Editability is an ownership and
synchronisation property. None is cached as `Draft`, `Valid`, `Compiled`, or `Locked`; doing so
without a versioned proof would become stale and falsely constrain or authorize later operations.

### 14.2 Concurrency Model

The C++ `Score` structure is the serialisable value representation. It contains no lock, supplies
no mutable thread-safety, and may be mutated only by one owner at a time. A `const Score` can be
read concurrently only when no thread can mutate that same value.

`ScoreDocument` is the implemented **single-writer, multiple-reader** ownership boundary. Copies of
one `ScoreDocument` are handles to the same logical document. `snapshot()` returns a
`shared_ptr<const Score>` to the current immutable version. A retained snapshot never changes and
remains valid after later commits, so queries, view computation, serialisation, and compilation may
run concurrently on it without retaining a document lock.

`transact(f)` has the following state transition for current snapshot (S_v):

1. Acquire the document's writer serializer and retain (S_v).
2. Copy (S_v) to a private candidate (C); readers continue to obtain (S_v).
3. Invoke `f(C)`. The callable returns `Result<T>`, may compose lower-level mutation routines, and
   must not recursively transact on the same document.
4. On a callable error or failed structural validation, discard (C). Snapshot identity and
   version remain exactly (S_v).
5. If `v` is the maximum `uint64` value, reject with `ArithmeticOverflow` before invoking `f`.
6. Otherwise retain the union of (S_v)'s and (C)'s represented/reserved identities, set
   `C.version = v + 1`, construct an immutable snapshot, and replace the current snapshot under one
   exclusive commit lock. Restoring prior content through a transaction therefore cannot erase
   identities retired in a later committed state.

The version increment denotes one externally visible transaction, even if its candidate work
composes several raw mutations. The callable must confine transaction-dependent side effects to
the candidate; effects on caller-owned objects are not rollback-managed by `ScoreDocument`.

**Rationale**: Musical documents are authored sequentially — a composer (human or agent) makes one change at a time. Concurrent writes introduce merge conflicts whose resolution in a musical context is underdetermined (two agents inserting different notes at the same position have no automatic reconciliation). The single-writer model avoids this class of problems entirely.

The implementation uses copy-on-write plus two distinct synchronization roles: a writer mutex
serialises all candidate work, while a shared snapshot lock protects only pointer acquisition and
the final pointer replacement. Consequently no two transactions overlap, a reader sees either
(S_v) or (S_{v+1}), no partial candidate state is observable, and slow candidate validation does
not prevent readers from retaining the last committed version. Process-global ID allocators used
by raw mutation helpers are atomic so transactions on independent documents do not race; this does
not make concurrent raw mutation of the same `Score` valid.

### 14.3 Error Codes

The Score IR extends the Sunny error code system (`types/music_types` §ErrorCode) with the following ranges:

| Range | Category | Description |
|-------|----------|-------------|
| 5000–5099 | Document structure | Invalid hierarchy, missing parts, measure count mismatch |
| 5100–5199 | Event validation | Invalid offset, overlapping events, tie mismatch |
| 5200–5299 | Temporal | Invalid ScoreTime, TempoMap gaps, tick conversion error |
| 5300–5399 | Annotation | Stale harmonic layer, inconsistent orchestration |
| 5400–5499 | Mutation | Invalid mutation parameters, invariant violation |
| 5500–5599 | Compilation | Compiler-specific errors (missing preset, unmapped articulation) |
| 5600–5699 | Serialisation | Schema version mismatch, corruption, validation failure on load |

Each diagnostic produced by validation (§10) carries an error code from this range, enabling programmatic error handling without string parsing.

### 14.4 ChordSymbol and HarmonicAnnotation Relationship

ChordSymbol events (§4.8) and HarmonicAnnotation entries (§6.2) both represent harmonic information, but at different levels of the hierarchy and for different purposes:

- **ChordSymbol** is a *notational element*: it appears in a voice within a measure and is rendered in the printed score (lead-sheet chord symbols above the staff). It is part of the document's content.
- **HarmonicAnnotation** is an *analytical element*: it resides in the global HarmonicAnnotationLayer and represents a harmonic analysis that may or may not align with explicit ChordSymbol events. It is part of the document's metadata.

When both are present at the same time position, the HarmonicAnnotation's `chord` field should be consistent with the ChordSymbol's root and quality. Inconsistency is reported as a validation warning (M8) but is not an error — the annotation may represent a different analytical interpretation (e.g., a passing chord symbol over a sustained harmony, or a reinterpretation in a different key context).

**Derivation**: When no explicit ChordSymbols exist, the HarmonicAnnotationLayer can be populated by running harmonic analysis on the pitch content. When ChordSymbols do exist, they serve as strong hints for the analysis. The relationship is advisory, not enforced.

---

## 15. Cross-Document References

### 15.1 Dependencies on Theory Spec

The Score IR depends on the Theory Spec for all musical primitives. Changes to the Theory Spec that affect the types listed in §0.4 require corresponding updates to the Score IR.

The dependency is one-directional: the Theory Spec does not depend on or reference the Score IR. The Theory Spec defines *what music is*; the Score IR defines *what a specific piece of music contains*.

### 15.2 Dependencies on Infrastructure

The Score IR depends on the existing Sunny infrastructure through abstract interface contracts. The Score IR does not depend on concrete implementations; it depends on the following behavioural interfaces:

| Interface | Contract | Provided By |
|-----------|----------|-------------|
| `CompilationTarget` | **Pre**: valid Score IR document (full validation passes). **Post**: output or acknowledged operations plus explicit capability diagnostics are produced deterministically. **Invariant**: Score IR is not modified. | AbletonCompiler, MidiCompiler, MusicXmlCompiler, LilyPondCompiler |
| `ProjectCompiler` | **Pre**: Score, Timbre, and Mix local validation and exact Part correspondence pass. **Post**: a target-read-only exact plan or a guarded journalled apply resolves downstream Live targets through the Score-derived Part map. | `plan_project_to_ableton`, `apply_project_ableton_plan` |
| `BridgeTransport` | **Pre**: connection is established (ConnectionState::Connected). **Post**: command is delivered and acknowledged, or an error is returned within the configured timeout. | LOM Bridge (`infrastructure/ableton/lom_protocol`) |
| `ToolRegistry` | **Pre**: tool function signature matches MCP schema. **Post**: tool is invocable by MCP clients. | MCP Server |

These contracts are structural requirements for compilation and type resolution. Implementations may be injected at construction time, enabling isolated testing of the Score IR without a running DAW or MCP server.

### 15.3 Compilation Pipeline

The full pipeline from agent intent to sounding music:

```
Agent Intent
    ↓ (MCP tool calls)
Score IR + Timbre IR + Mix IR
    ↓ project validation (local rules + exact Part correspondence)
Project compiler (Score order → authoritative Part-to-track map)
    ↓ Score → Timbre → Mix deployment
Ableton Live Set

Score IR ──→ MIDI event data / MusicXML / LilyPond
```

The three validated IR documents are the source of truth for their disjoint domains throughout this
cycle: Score for musical structure/performance intent, Timbre for source/device realization, and Mix
for routing/level/spatial realization. `ProjectView` and its exact `PartId` bijections are the
authoritative production composition. The agent does not bypass that composition by silently
editing target state and then treating the target as source truth.

---

## 16. Invariant Summary

This section states the full normative document ideal. The current validator enforces the applicable structural and reference checks, while annotations may be partial and only mutations that expose an inverse participate in undo. The implemented runtime profile in §0.5 is authoritative for callable guarantees.

### 16.1 Structural Invariants

1. The Score has at least one Part.
2. Every Part has exactly `total_bars` Measures.
3. Every Measure has at least one Voice.
4. Every Voice's events fill exactly the measure duration.
5. Events within a Voice are non-overlapping and ordered by offset.
6. TempoMap, KeySignatureMap, and TimeSignatureMap each have at least one entry at the score's start.
7. Section spans at the same nesting level are non-overlapping.
8. Child sections are contained within parent sections.
9. Tied notes have matching pitch at adjacent temporal positions.
10. Tuplet events sum to their declared span.
11. Event offsets are within [0, measure_duration) and event spans do not exceed the measure boundary.
12. A BeamGroup is one explicit primary beam with globally unique identity, at least two ordered
    contiguous measured members of eligible written duration, and exact NoteGroup back-references;
    under-specified secondary-beam metadata is not compilable.
13. Every KeySignature has a closed ordered scale profile; registry names match their definitions,
    and a standard mode's tonic, definition, and stored fifths count are coherent.
14. Every Voice belongs to exactly one configured Part staff, and differentiated initial clefs are complete.
15. Every stored single-identity notation span has coherent endpoints in its Voice or Staff scope.
16. Every lyric verse lane has unambiguous onset ownership, closed word state, and a derivable
    melisma endpoint.

### 16.2 Compilation Invariants

17. ScoreTime → AbsoluteBeat is monotonically increasing.
18. AbsoluteBeat → TickTime is monotonically increasing.
19. TickTime rounding error does not exceed 0.5 ticks per bar.
20. For every valid Score IR, each compiler produces exactly one output.
21. Concert pitch storage: all Note pitches are sounding pitch, never written pitch.

### 16.3 Annotation Layer Invariants

22. The optional HarmonicAnnotation layer is ordered and non-overlapping; each present entry has
    a positive in-score span and the complete payload/inversion/key identity of §6.2. No total-score
    coverage is required.
23. OrchestrationAnnotation entries for a single part are non-overlapping within that part.
24. Stale annotation regions are tracked and reported by validation.

### 16.4 Mutation Invariants

25. Every mutation has a computable inverse.
26. Applying a mutation followed by its inverse restores the previous document state exactly.
27. The version counter increases monotonically and is never reused (including across undo/redo).
28. For one `ScoreDocument`, at most one transaction callable is in progress at any instant.
29. A retained `ScoreDocument` snapshot is immutable; a reader observes one complete committed
    version even while a later candidate is being prepared.
30. `ScoreDocument` rejection preserves the current snapshot identity and version. Raw mutation
    rejection changes neither Score state nor version nor undo/redo history.
31. A successful topology-changing mutation preserves exact measured tiling and paired-endpoint coherence.
32. Bar splices transform every position-bearing map, nested span, directive, and stale-analysis region under one half-open splice function.
33. A meter change retimes only derived rest coverage; it never clips musical or point content.
34. Semantic loss forced by the current identity model is returned as a typed mutation diagnostic.

---

## Appendix A: Standard Instrument Library

Reference definitions for common orchestral instruments. Parts created through the MCP tools
take their sounding range, transposition, and default clef from this table (§12.2); an unlisted
instrument receives the full MIDI range.

| Instrument | Class | Transposition | Range (absolute) | Range (comfortable) | Clef | Staves |
|-----------|-------|---------------|-----------------|--------------------|----|--------|
| Piccolo | Woodwinds.Piccolo | +P8 (sounds octave higher) | D5–C8 (written D4–C7) | D5–A7 | Treble | 1 |
| Flute | Woodwinds.Flute | Concert | C4–D7 | C4–C7 | Treble | 1 |
| Oboe | Woodwinds.Oboe | Concert | B♭3–A6 | C4–G6 | Treble | 1 |
| English Horn | Woodwinds.EnglishHorn | −P5 | E3–C6 (written B3–G6) | B3–A5 | Treble | 1 |
| Clarinet in B♭ | Woodwinds.Clarinet | −M2 | D3–B♭6 (written E3–C7) | E3–G6 | Treble | 1 |
| Bass Clarinet | Woodwinds.BassClarinet | −M9 | D♭2–G5 (written E♭3–A6) | E♭3–E5 | Treble | 1 |
| Bassoon | Woodwinds.Bassoon | Concert | B♭1–E♭5 | C2–C5 | Bass | 1 |
| Contrabassoon | Woodwinds.Contrabassoon | −P8 | B♭0–B♭3 (written B♭1–B♭4) | C1–A3 | Bass | 1 |
| French Horn in F | Brass.FrenchHorn | −P5 | B1–F5 (written F♯2–C6) | F2–C5 | Treble | 1 |
| Trumpet in B♭ | Brass.Trumpet | −M2 | E3–B♭5 (written F♯3–C6) | G3–G5 | Treble | 1 |
| Trombone | Brass.Trombone | Concert | E2–B♭4 | A2–G4 | Bass | 1 |
| Bass Trombone | Brass.BassTrombone | Concert | B♭1–G4 | C2–F4 | Bass | 1 |
| Tuba | Brass.Tuba | Concert | D1–F4 | F1–D4 | Bass | 1 |
| Timpani | Percussion.Timpani | Concert | D2–C4 (standard set) | D2–C4 | Bass | 1 |
| Violin | Strings.Violin | Concert | G3–E7 | G3–B6 | Treble | 1 |
| Viola | Strings.Viola | Concert | C3–E6 | C3–A5 | Alto | 1 |
| Cello | Strings.Cello | Concert | C2–A5 | C2–E5 | Bass | 1 |
| Double Bass | Strings.DoubleBass | −P8 (sounds octave lower) | E1–G4 (written E2–G5) | E2–D4 | Bass | 1 |
| Harp | Strings.Harp | Concert | C♭1–G♯7 | C1–G7 | Treble+Bass | 2 |
| Piano | Keyboard.Piano | Concert | A0–C8 | A0–C8 | Treble+Bass | 2 |

---

## Appendix B: Glossary

| Term | Definition |
|------|-----------|
| **AbsoluteBeat** | Cumulative beat position from the start of the score |
| **BeamGroup** | A set of events sharing a beam in notation (§4.10) |
| **Compilation** | Deterministic transformation of Score IR to a rendering target |
| **Direction** | A zero-duration annotation attached to a time point |
| **Event** | The atomic unit of musical content (note, rest, chord symbol, or direction) |
| **Hairpin** | A gradual dynamic change (crescendo or diminuendo) |
| **HarmonicAnnotation** | A chord analysis entry in the harmonic layer |
| **HarmonicFunction** | Functional classification of a chord: Tonic, Predominant, Dominant, or Ambiguous (§6.2) |
| **Measure** | A single bar within a part |
| **MCP** | Model Context Protocol; the JSON-RPC interface for agent interaction |
| **NoteGroup** | One or more simultaneous notes sharing onset and duration |
| **NoteHead** | Non-standard notehead shape for special notation (§4.11) |
| **OrchestrationLayer** | Annotations describing the textural role of each part |
| **Part** | A single instrumental line in the score |
| **PartDirective** | A scoped performance instruction for an entire part |
| **PositiveRational** | A canonical strictly positive rational rate with checked dynamic construction; used for BPM (§2.3) |
| **RealTime** | Clock time in seconds, derived from AbsoluteBeat and TempoMap |
| **Region** | A bounded span of score time, optionally restricted to a subset of parts |
| **RenderingConfig** | DAW-specific configuration for compiling a part |
| **Score IR** | The intermediate representation; the complete document model |
| **ScoreTime** | The primary temporal coordinate: (bar number, beat within bar) |
| **Section** | A labelled formal unit in the score's structure |
| **SpelledPitch** | A pitch with explicit letter name, accidental, and octave |
| **Stale** | An annotation region whose backing note content has changed since the annotation was computed |
| **TechnicalDirection** | Instrument-specific performance instruction (§4.12) |
| **TempoMap** | A piecewise function from score time to tempo |
| **TexturalRole** | The function a part serves in the texture (melody, bass, harmonic fill, etc.) |
| **TickTime** | Discretised time in MIDI pulses |
| **TupletContext** | Grouping metadata for tuplet notation |
| **Validation** | Checking the Score IR against its invariants |
| **View** | A derived, read-only projection of the score (piano reduction, short score, etc.) |
| **Voice** | A monophonic stream of events within a measure |

---

*End of specification.*
