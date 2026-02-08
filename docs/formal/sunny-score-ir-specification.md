# Sunny Score IR — Formal Specification

**Version:** 0.1.0-draft  
**Date:** 2026-02-08  
**Status:** Initial specification; subject to iterative refinement  
**Dependency:** Sunny Engine Formal Specification v0.1.0 (the "Theory Spec")

---

## 0. Preamble

### 0.1 Purpose

This document defines the formal specification for the Sunny Score IR (Intermediate Representation), a hierarchical document model for representing complete musical works as structured, queryable, manipulable objects. The Score IR is the authoritative source document from which all downstream renderings — Ableton Live sessions, MIDI files, notation, audio — are derived.

The Score IR occupies a distinct architectural layer from the Sunny music theory engine. The theory engine provides the vocabulary: pitch classes, intervals, chords, scales, voice-leading constraints, formal templates. The Score IR provides the document: the concrete instantiation of that vocabulary into a particular work, with particular instruments, particular notes at particular times, particular dynamics and articulations, arranged into a particular formal structure. The relationship is analogous to that between a programming language specification and a source file written in that language.

### 0.2 Design Objectives

The Score IR is designed to satisfy five constraints simultaneously:

1. **Completeness**: Every property of a musical work that affects its sounding result or its structural interpretation must be representable. A Score IR document must contain sufficient information to produce both a performance (via MIDI/DAW rendering) and a conventional score (via notation rendering) without supplementary data.

2. **Semantic fidelity**: The representation preserves compositional intent, not merely acoustic outcome. A staccato quarter note and a short eighth note followed by an eighth rest may produce identical MIDI output, but they carry different meanings and must remain distinguishable in the IR.

3. **Structural navigability**: An agent must be able to query the document at any level of the hierarchy — "what is the harmony at bar 47?", "which instruments carry the melody in the development section?", "show me all instances of the opening motif" — without scanning the entire document linearly.

4. **Incremental mutability**: An agent must be able to modify the document at any granularity — insert a note, reorchestrate a passage, restructure a section — while the IR maintains its internal consistency invariants automatically or reports violations explicitly.

5. **Deterministic compilation**: The mapping from Score IR to each rendering target (Ableton session, MIDI file, MusicXML, LilyPond) must be a total function: every valid Score IR document produces exactly one output in each target format, and the output is reproducible.

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

---

## 1. Document Hierarchy

### 1.1 Structural Overview

The Score IR is a tree with five principal levels. Each level owns its children and provides the context within which its children are interpreted.

```
Score
 ├── ScoreMetadata
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

**Region address**: A bounded span of score time, optionally restricted to a subset of parts. Example: `Region(parts=[0,1,2], from=ScoreTime(33,Beat(1,1)), to=ScoreTime(64,Beat(1,1)))` selects all content in parts 0–2 from bar 33 to bar 64. Regions are the primary operand for bulk operations (reorchestration, transposition, dynamic scaling).

---

## 2. Score Level

### 2.1 Score

**Definition 2.1.1**. A *Score* is the root node of the document hierarchy.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<Score>` | Unique document identifier |
| `metadata` | `ScoreMetadata` | Descriptive metadata |
| `tempo_map` | `TempoMap` | Tempo as a function of score time |
| `key_map` | `KeySignatureMap` | Key signatures as a function of score time |
| `time_map` | `TimeSignatureMap` | Time signatures as a function of score time |
| `section_map` | `SectionMap` | Formal structure annotations |
| `rehearsal_marks` | `Vec<RehearsalMark>` | Named navigation points |
| `parts` | `Vec<Part>` | Ordered list of instrumental parts |
| `harmonic_annotations` | `HarmonicAnnotationLayer` | Global harmonic analysis (§6) |
| `orchestration_annotations` | `OrchestrationLayer` | Textural role assignments (§7) |
| `version` | `u64` | Monotonically increasing edit counter |

**Invariants**:
- `parts` is non-empty.
- All parts have the same total duration (measured in score time); if a part has fewer notated events, it is padded with implicit rests.
- `tempo_map`, `key_map`, and `time_map` are defined for the entire duration of the score (from bar 1 through the final bar).

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
| `bpm` | `Beat` | Tempo in beats per minute (exact rational) |
| `beat_unit` | `BeatUnit` | Which note value receives the beat (quarter, dotted quarter, half, etc.) |
| `transition` | `TempoTransition` | How the previous tempo changes to this one |

**TempoTransition** variants:

| Variant | Semantics |
|---------|-----------|
| `Immediate` | Instantaneous tempo change |
| `Linear(duration: Beat)` | Linear interpolation (accelerando/ritardando) from previous tempo over the given duration |
| `MetricModulation { old_unit: BeatUnit, new_unit: BeatUnit }` | Previous subdivision becomes new beat; BPM is derived from the ratio |

**BeatUnit** enumeration: `Whole`, `Half`, `DottedHalf`, `Quarter`, `DottedQuarter`, `Eighth`, `DottedEighth`, `Sixteenth`.

The BeatUnit determines the denominator of the BPM fraction. A tempo of "dotted quarter = 120" in 6/8 means 120 dotted-quarter notes per minute; the effective eighth-note rate is 360 per minute.

**Resolution**: Given a ScoreTime *t*, the TempoMap resolves to an instantaneous BPM by:
1. Finding the latest TempoEvent at or before *t*.
2. If its transition is `Immediate`, the BPM is the event's bpm.
3. If its transition is `Linear(d)`, the BPM is linearly interpolated between the previous event's bpm and this event's bpm, over the duration *d*.
4. If its transition is `MetricModulation`, the BPM is computed from the ratio of old and new beat units applied to the preceding tempo.

**Invariant**: The TempoMap has at least one entry at ScoreTime(1, Beat(1,1)) (the beginning of the piece). BPM values are strictly positive.

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

**Derivation**: The `accidentals` field is derivable from `root` and `mode` for standard key signatures, but is stored explicitly to support non-standard or theoretical key signatures (e.g., more than 7 sharps or flats).

**Invariant**: At least one entry at ScoreTime(1, Beat(1,1)).

### 2.5 TimeSignatureMap

**Definition 2.5.1**. The *TimeSignatureMap* is a piecewise-constant function from bar number to time signature.

**Representation**: An ordered sequence of entries:

| Field | Type | Description |
|-------|------|-------------|
| `bar` | `u32` | Bar number where this time signature takes effect |
| `time_signature` | `TimeSignature` | The time signature (from Theory Spec §9.2) |

The time signature persists until the next entry. Every bar between two consecutive entries has the time signature of the earlier entry.

**Derived property**: The *duration of a measure* in beats is computable from the TimeSignature: `numerator / denominator` expressed as a Beat value. For 4/4: Beat(4, 4) = Beat(1, 1) whole notes, or equivalently Beat(4, 1) quarter notes depending on the beat unit convention. The Score IR normalises measure duration to quarter-note beats: a 4/4 bar has duration Beat(4, 1); a 6/8 bar has duration Beat(3, 1) in dotted-quarter beats or Beat(6, 1) in eighth-note beats. The normalisation convention is: *measure duration in quarter notes* = Beat(numerator, denominator) × 4.

Example: 6/8 → Beat(6, 8) × 4 = Beat(3, 1) quarter notes. 3/4 → Beat(3, 4) × 4 = Beat(3, 1) quarter notes. Both are 3 quarter-note durations, which is correct.

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
- Sections at the same nesting level do not overlap.
- Every child's time span is contained within its parent's time span.
- The union of top-level sections covers the entire score duration (no gaps).

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

**Invariant**: `measures.len() == score.metadata.total_bars`. Every part has exactly one Measure for every bar in the score, even if that measure contains only rests.

### 3.2 PartDefinition

**Definition 3.2.1**. The *PartDefinition* specifies the instrument, its capabilities, and its rendering configuration.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `name` | `String` | Display name (e.g., "Violin I", "Alto Saxophone", "Pad Synth") |
| `abbreviation` | `String` | Abbreviated name for condensed scores (e.g., "Vln. I", "A. Sax.") |
| `instrument_class` | `InstrumentClass` | Classification for orchestration logic |
| `transposition` | `Interval` | Concert-to-written transposition interval |
| `clef` | `Clef` | Default clef |
| `range` | `PitchRange` | Playable range (soft limits) and comfortable range (hard limits) |
| `articulation_vocabulary` | `Vec<ArticulationType>` | Articulations this instrument supports |
| `staff_count` | `u8` | Number of staves (1 for most; 2 for piano, organ, harp) |
| `rendering` | `RenderingConfig` | DAW-specific rendering parameters |

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
| `instrument_preset` | `Option<String>` | Ableton instrument or plugin preset path |
| `midi_channel` | `u8` | MIDI channel (1–16) |
| `articulation_map` | `Map<ArticulationType, ArticulationMapping>` | How each articulation compiles to MIDI |
| `expression_cc` | `u8` | CC number for expression/dynamics (default: CC 11) |
| `pan` | `Option<f32>` | Stereo pan position (−1.0 to 1.0) |
| `group` | `Option<String>` | Ableton group track assignment |

**ArticulationMapping** variants:

| Variant | Description |
|---------|-------------|
| `Keyswitch(SpelledPitch)` | Send a keyswitch note before the articulated note |
| `CC(u8, u8)` | Send a CC message (controller number, value) |
| `VelocityLayer(u8, u8)` | Map to a velocity range (min, max) |
| `NoteDurationScale(f32)` | Scale note duration by a factor (e.g., staccato = 0.5) |
| `ProgramChange(u8)` | Send a program change message |
| `Combined(Vec<ArticulationMapping>)` | Multiple simultaneous mappings |

### 3.3 PartDirective

**Definition 3.3.1**. A *PartDirective* is a scoped instruction that applies to an entire part over a time range.

| Field | Type | Description |
|-------|------|-------------|
| `start` | `ScoreTime` | Effective from |
| `end` | `ScoreTime` | Effective until |
| `directive` | `DirectiveType` | The instruction |

**DirectiveType** examples: `Mute`, `Solo`, `ConSordino`, `SenzaSordino`, `Pizzicato`, `Arco`, `Divisi(u8)`, `Tutti`, `Tacet`, `ColLegno`, `SulPonticello`, `SulTasto`, `HalfPedal`, `SustainingPedal`, `UnaCorda`, `TreCorde`.

These directives affect rendering (sordino might map to a keyswitch or a filter change in the DAW) and notation (they produce text instructions in the printed score).

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
- `voices` is non-empty (at minimum, one voice containing rests).
- The total notated duration of each voice equals the measure duration derived from the active time signature. If events do not fill the measure, the voice is padded with implicit rests; if events exceed the measure duration, a validation error is raised.

### 4.2 Voice

**Definition 4.2.1**. A *Voice* is a monophonic (or chordal) stream of events within a measure. Multiple voices within a single measure represent polyphonic writing on a single staff (e.g., soprano and alto on the treble staff of a choral score, or two independent melodic lines in a divisi string part).

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `voice_index` | `u8` | 0-indexed voice number within the measure |
| `events` | `Vec<Event>` | Ordered sequence of events |

**Invariant**: Events are ordered by start time. No two note events within the same voice overlap in time (this is the monophonic constraint within a voice; chords are represented as simultaneous notes within a single event, not as overlapping independent notes).

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

### 4.4 NoteGroup

**Definition 4.4.1**. A *NoteGroup* represents one or more simultaneous notes sounding at the same onset and sharing the same rhythmic value. A single melodic note is a NoteGroup of size 1. A chord played by a single instrument (e.g., a piano chord) is a NoteGroup of size > 1.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `notes` | `Vec<Note>` | One or more notes (non-empty) |
| `duration` | `Beat` | Notated duration (exact rational) |
| `tuplet_context` | `Option<TupletContext>` | Enclosing tuplet, if any |
| `beam_group` | `Option<Id<BeamGroup>>` | Beam grouping identifier |
| `slur_start` | `bool` | Whether a slur begins here |
| `slur_end` | `bool` | Whether a slur ends here |

### 4.5 Note

**Definition 4.5.1**. A *Note* is a single pitched event with full performance metadata.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `pitch` | `SpelledPitch` | Concert pitch with enharmonic spelling |
| `velocity` | `Velocity` | Attack intensity |
| `articulation` | `Option<Articulation>` | Articulation marking |
| `dynamic` | `Option<Dynamic>` | Dynamic marking at this note |
| `ornament` | `Option<Ornament>` | Ornamental figure |
| `tie_forward` | `bool` | Whether this note is tied to the next occurrence of the same pitch |
| `grace` | `Option<GraceType>` | Grace note classification, if this is a grace note |
| `technical` | `Vec<TechnicalDirection>` | Instrument-specific performance instructions |
| `lyric` | `Option<String>` | Lyric syllable (for vocal parts) |
| `notation_head` | `Option<NoteHead>` | Non-standard notehead (diamond, cross, slash, etc.) |

#### 4.5.1 Velocity

**Definition 4.5.2**. Velocity is represented as both a semantic dynamic level and a numeric value, to separate compositional intent from rendering output.

| Field | Type | Description |
|-------|------|-------------|
| `written` | `Option<DynamicLevel>` | Semantic dynamic (if a dynamic marking is present at this note) |
| `value` | `u8` | MIDI velocity (0–127), derived from context if `written` is absent |

The numeric value is resolved during compilation by applying the current dynamic level, any hairpin in effect, the instrument's dynamic curve, and any accent or sforzando articulation.

#### 4.5.2 Articulation

**Definition 4.5.3**. Articulation types, grouped by family:

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
- `Trill { interval: Interval }` — rapid alternation with a note at the given interval
- `Mordent { inverted: bool }` — single alternation
- `Turn { inverted: bool }` — four-note figure
- `Tremolo { strokes: u8 }` — unmeasured repetition (1 = eighth-note, 2 = sixteenth, 3 = thirty-second)
- `Harmonic { type: HarmonicType }` — natural or artificial harmonic
- `GlissandoStart` / `GlissandoEnd` — continuous pitch slide between two notes
- `SnapPizzicato` — (Bartók pizzicato) for strings
- `DownBow` / `UpBow` — bowing direction for strings
- `OpenString` / `Stopped` — for strings and brass
- `Muted` — with mute applied (if not already a PartDirective)
- `BendUp(cents: i16)` / `BendDown(cents: i16)` — pitch bend

Each articulation has a default rendering behaviour (§9) that can be overridden per instrument in the RenderingConfig.

#### 4.5.3 Dynamic

**Definition 4.5.4**. Dynamic markings.

**Instantaneous dynamics** (`DynamicLevel`):

| Level | Name | Default Velocity Range |
|-------|------|----------------------|
| `pppp` | Pianississimo | 8–15 |
| `ppp` | Pianissimo | 16–31 |
| `pp` | Piano | 32–47 |
| `p` | MezzoPiano | 48–63 |
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

#### 4.5.4 GraceType

**Definition 4.5.5**. Grace notes are pre-beat or on-beat ornamental notes.

| Variant | Description | Rendering |
|---------|-------------|-----------|
| `Acciaccatura` | Crushed grace note (slashed stem) | Very short, before the beat |
| `Appoggiatura` | Leaning grace note (no slash) | Takes time from the following note |

Grace notes have a written duration (for notation) but their sounding duration is determined by convention and rendering policy. The Score IR stores them with their written duration; the compiler resolves their actual timing.

#### 4.5.5 Ornament

**Definition 4.5.6**. Ornament types:

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

### 4.8 ChordSymbol

**Definition 4.8.1**. A *ChordSymbol* is a harmonic annotation in lead-sheet or Roman numeral form, attached to a time position. It does not produce sound directly; it annotates the harmonic context for the purpose of analysis, improvisation, or agent reasoning.

| Field | Type | Description |
|-------|------|-------------|
| `root` | `SpelledPitch` | Root of the chord (letter + accidental) |
| `quality` | `ChordQuality` | From the Theory Spec chord quality registry |
| `bass` | `Option<SpelledPitch>` | Slash bass note, if different from root |
| `roman` | `Option<String>` | Roman numeral representation in current key |
| `extensions` | `Vec<String>` | Textual extensions (e.g., "add9", "♯11") |

### 4.9 TupletContext

**Definition 4.9.1**. A *TupletContext* groups a set of events under a tuplet bracket.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<TupletContext>` | Shared among all events in this tuplet |
| `actual` | `u8` | Number of notes in the tuplet (*m* in *m*:*n*) |
| `normal` | `u8` | Number of notes in the normal division (*n* in *m*:*n*) |
| `normal_type` | `Beat` | Duration of each normal note |
| `nested_in` | `Option<Id<TupletContext>>` | Parent tuplet if nested |

The total span of the tuplet is `normal × normal_type`. Each event within the tuplet has its written duration; the sounding duration is scaled by the factor `normal / actual`.

**Example**: A quarter-note triplet in 4/4. actual = 3, normal = 2, normal_type = Beat(1, 1) quarter note. Total span = 2 quarter notes. Each note's sounding duration = (2/3) × quarter note.

---

## 5. Temporal Coordinate System

### 5.1 ScoreTime

**Definition 5.1.1**. *ScoreTime* is the primary temporal coordinate within the Score IR.

| Field | Type | Description |
|-------|------|-------------|
| `bar` | `u32` | 1-indexed bar number |
| `beat` | `Beat` | Position within the bar (Beat(0,1) = start of bar; Beat(1,1) = second quarter note in 4/4) |

**Ordering**: ScoreTime values are totally ordered. (*b*₁, *β*₁) < (*b*₂, *β*₂) iff *b*₁ < *b*₂, or *b*₁ = *b*₂ and *β*₁ < *β*₂.

### 5.2 AbsoluteBeat

**Definition 5.2.1**. *AbsoluteBeat* is the cumulative beat position from the beginning of the score, computed by summing measure durations:

AbsoluteBeat(ScoreTime(*b*, *β*)) = ∑ᵢ₌₁^{*b*−1} duration(measure *i*) + *β*

This is a monotonically increasing function of ScoreTime and provides a single-axis coordinate suitable for alignment across parts.

### 5.3 RealTime

**Definition 5.3.1**. *RealTime* is the clock time (in seconds) from the beginning of the piece, computed by integrating the TempoMap over AbsoluteBeat:

RealTime(*t*) = ∫₀^{AbsoluteBeat(*t*)} (60 / BPM(τ)) dτ

For piecewise-constant tempos, this is a sum of linear segments. For linear tempo transitions (accelerando/ritardando), the integral has a closed-form solution.

RealTime is used for DAW compilation (MIDI event timestamps, automation curve timing) but is not used for internal Score IR operations, which operate in ScoreTime.

### 5.4 Tick Time

**Definition 5.4.1**. *TickTime* is the discretised time coordinate used by MIDI and DAW transport, measured in pulses per quarter note (PPQ). Given a PPQ resolution *R* (default: 480):

TickTime(*t*) = AbsoluteBeat(*t*) × *R*

Since AbsoluteBeat is an exact rational, TickTime is also an exact rational. However, MIDI files require integer ticks; the Score IR compiler rounds to the nearest integer tick and tracks the accumulated rounding error to prevent drift.

**Invariant**: Cumulative rounding error in tick conversion never exceeds 0.5 ticks over any bar boundary. This is enforced by the compiler distributing rounding corrections across beats within each measure.

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
| `function` | `HarmonicFunction` | T, PD, D classification |
| `secondary_function` | `Option<String>` | e.g., "V/V", "viio/vi" |
| `non_chord_tones` | `Vec<NonChordToneAnnotation>` | Identified passing tones, suspensions, etc. |
| `cadence` | `Option<CadenceType>` | If this annotation participates in a cadence |
| `confidence` | `f32` | Confidence of the analysis (1.0 = certain, < 1.0 = heuristic) |

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

This is useful for focused work on a single section, and for comparing parallel passages (e.g., the exposition and recapitulation of a sonata).

---

## 9. Compilation

### 9.1 Compilation Model

*Compilation* is the deterministic transformation of a validated Score IR document into a rendering target. Each target has its own compiler. All compilers read the Score IR and produce output; no compiler modifies the Score IR.

```
Score IR ──┬──→ AbletonCompiler ──→ Ableton Live Session (via LOM bridge)
           ├──→ MidiCompiler ──→ Standard MIDI File (.mid)
           ├──→ MusicXmlCompiler ──→ MusicXML (.musicxml)
           ├──→ LilyPondCompiler ──→ LilyPond (.ly)
           └──→ AudioCompiler ──→ (future: audio rendering)
```

### 9.2 AbletonCompiler

**Definition 9.2.1**. The *AbletonCompiler* produces an Ableton Live session from the Score IR.

**Compilation steps**:

1. **Track creation**: For each Part, create an Ableton MIDI track via the LOM bridge. Assign the instrument preset from `RenderingConfig.instrument_preset`. Assign the track to a group if `RenderingConfig.group` is specified. Set pan from `RenderingConfig.pan`.

2. **Clip generation**: For each contiguous section (from the SectionMap), create an Ableton MIDI clip on the corresponding track. The clip's start and end times are computed from ScoreTime → TickTime (§5.4). Within the clip, each NoteGroup compiles to one or more MIDI note-on/note-off pairs:
   - Pitch: `midi(note.pitch)` from the Theory Spec.
   - Velocity: `note.velocity.value` (resolved; see §9.5).
   - Start: TickTime of the event offset within the clip.
   - Duration: Sounding duration after articulation adjustment (§9.4).

3. **Articulation rendering**: For each articulated note, apply the ArticulationMapping from the part's RenderingConfig:
   - Keyswitches are inserted as note-on/note-off pairs preceding the articulated note by a configurable lead time (default: 1 tick).
   - CC messages are inserted at the note's start time.
   - Duration scaling is applied to the note's MIDI duration.

4. **Dynamic rendering**: Hairpins and dynamic markings compile to CC automation lanes (using `RenderingConfig.expression_cc`). Instantaneous dynamics set the CC value at the event's time point. Hairpins produce linear CC ramps between start and end positions.

5. **Tempo rendering**: The TempoMap compiles to Ableton's master tempo automation. Immediate changes produce step automation; linear transitions produce ramp automation.

6. **Time signature rendering**: The TimeSignatureMap compiles to Ableton's time signature track.

7. **Section markers**: Section boundaries from the SectionMap compile to Ableton arrangement locators (named markers in the arrangement view).

8. **Part directives**: Directives such as `ConSordino` or `Pizzicato` compile to CC messages or keyswitches according to the ArticulationMapping, inserted at the directive's start time, with a restoration message at the end time.

### 9.3 MidiCompiler

**Definition 9.3.1**. The *MidiCompiler* produces a Standard MIDI File (SMF) Type 1.

**Mapping**:

| Score IR Element | MIDI Event |
|-----------------|------------|
| Part | Track |
| NoteGroup → Note | Note On / Note Off |
| TempoMap entry | Meta event: Set Tempo |
| TimeSignatureMap entry | Meta event: Time Signature |
| KeySignatureMap entry | Meta event: Key Signature |
| Dynamic (instantaneous) | CC message (expression) |
| Hairpin | Sequence of CC messages (interpolated) |
| Articulation (CC-mapped) | CC message |
| Articulation (keyswitch) | Note On / Note Off |
| PartDirective | CC or program change |
| RehearsalMark | Meta event: Marker |
| Section boundary | Meta event: Cue Point |

**Resolution**: 480 PPQ (configurable).

**Limitation**: MIDI does not preserve enharmonic spelling, articulation semantics, or orchestration metadata. The MIDI compiler is lossy with respect to these properties. The Score IR remains the source of truth.

### 9.4 Articulation Duration Resolution

**Definition 9.4.1** [C]. The sounding duration of a note is computed from its written duration and its articulation:

| Articulation | Duration Factor | Notes |
|-------------|----------------|-------|
| (none / legato) | 1.0 | Full written duration |
| Tenuto | 1.0 | Full duration, explicit |
| Portato | 0.75 | Between legato and staccato |
| Staccato | 0.50 | Half of written duration |
| Staccatissimo | 0.25 | Quarter of written duration |
| Accent | 1.0 | Duration unchanged; velocity affected |
| Marcato | 0.85 | Slightly separated |
| Fermata | configurable (1.5–2.5) | Held beyond written duration |

These factors are defaults. The RenderingConfig may override them per instrument, and the ArticulationMapping may replace them entirely with a different rendering strategy (e.g., keyswitch instead of duration scaling).

### 9.5 Velocity Resolution

**Definition 9.5.1** [C]. The MIDI velocity of a note is resolved by the following procedure:

1. **Base velocity**: Determined by the most recent DynamicLevel in the part. Use the midpoint of the DynamicLevel's velocity range (from §4.5.3).

2. **Hairpin adjustment**: If a Hairpin is active at the note's time position, interpolate linearly between the starting dynamic's velocity and the ending dynamic's velocity based on the note's relative position within the hairpin.

3. **Articulation adjustment**:
   - Accent: velocity += 20 (clamped to 127).
   - Marcato: velocity += 30 (clamped to 127).
   - Sforzando: velocity = min(127, base + 40).
   - FortePiano: attack velocity as sforzando; subsequent velocity drops to piano level.

4. **Dynamic balance adjustment**: If OrchestrationLayer assigns the part a DynamicBalance:
   - Foreground: no adjustment.
   - MiddleGround: velocity × 0.85.
   - Background: velocity × 0.70.

5. **Clamp**: Result is clamped to [1, 127].

### 9.6 MusicXmlCompiler

**Definition 9.6.1**. The *MusicXmlCompiler* produces MusicXML 4.0 output.

This compiler is the most information-preserving: MusicXML supports enharmonic spelling, articulations, dynamics, lyrics, clef changes, tuplets, beaming, slurs, ties, rehearsal marks, and multi-voice notation. The compilation is a structural mapping from the Score IR hierarchy to MusicXML elements.

| Score IR | MusicXML |
|----------|----------|
| Score | `<score-partwise>` |
| Part | `<part>` |
| Measure | `<measure>` |
| Voice | `<voice>` elements within a measure |
| NoteGroup (single note) | `<note>` |
| NoteGroup (chord) | First `<note>`, subsequent `<note>` with `<chord/>` |
| Rest | `<note>` with `<rest/>` |
| SpelledPitch | `<pitch>` with `<step>`, `<alter>`, `<octave>` |
| Duration (Beat) | `<duration>` (integer divisions) + `<type>` (note type) |
| Articulation | `<articulations>` or `<technical>` within `<notations>` |
| Dynamic | `<dynamics>` within `<direction>` |
| Hairpin | `<wedge>` within `<direction>` |
| Tie | `<tie>` and `<tied>` |
| Slur | `<slur>` within `<notations>` |
| TupletContext | `<tuplet>` within `<notations>` + `<time-modification>` |
| KeySignature | `<key>` within `<attributes>` |
| TimeSignature | `<time>` within `<attributes>` |
| Clef | `<clef>` within `<attributes>` |
| TempoMap entry | `<sound tempo="...">` + `<direction>` with tempo text |
| RehearsalMark | `<rehearsal>` within `<direction>` |
| Section boundary | `<bookmark>` or rehearsal marks |
| Transposition | `<transpose>` in `<attributes>` |

### 9.7 LilyPondCompiler

**Definition 9.7.1**. The *LilyPondCompiler* produces LilyPond input files (.ly) for engraving-quality PDF score generation.

The compilation maps the Score IR hierarchy to LilyPond's textual music notation language. LilyPond's expressive power exceeds MIDI and approaches MusicXML; it supports fine-grained control over layout, spacing, and typography.

---

## 10. Validation

### 10.1 Validation Model

The Score IR maintains a set of invariants that are checked by validation. Validation is performed:
- After construction (initial validation).
- After each mutation (incremental validation of affected regions).
- Before compilation (full validation).

Validation produces a list of diagnostics, each classified by severity:

| Severity | Meaning | Effect |
|----------|---------|--------|
| `Error` | Invariant violation; document is structurally inconsistent | Compilation blocked |
| `Warning` | Potential problem; document is valid but may not render as intended | Compilation proceeds; diagnostic reported |
| `Info` | Observation for the agent's consideration | No effect on compilation |

### 10.2 Structural Validation Rules

| Rule | Severity | Description |
|------|----------|-------------|
| S1 | Error | Every part has exactly `total_bars` measures |
| S2 | Error | Every voice's events fill exactly the measure duration |
| S3 | Error | No overlapping events within a voice |
| S4 | Error | TempoMap has an entry at bar 1 |
| S5 | Error | TimeSignatureMap has an entry at bar 1 |
| S6 | Error | KeySignatureMap has an entry at bar 1 |
| S7 | Error | All tied notes have matching pitch and adjacent positions |
| S8 | Error | Tuplet events sum to the tuplet's total span |
| S9 | Error | Section spans do not overlap at the same nesting level |
| S10 | Error | Section hierarchy is properly nested |
| S11 | Error | Tone row (if present) is a valid permutation of 12 pitch classes |

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
| R1 | Warning | Part has no RenderingConfig.instrument_preset (will use default) |
| R2 | Warning | Articulation used but not present in part's articulation_vocabulary |
| R3 | Warning | Articulation used but no ArticulationMapping defined for it |
| R4 | Info | Grace note duration exceeds half the following note's duration |
| R5 | Warning | TickTime rounding error exceeds 0.5 ticks for a bar |

---

## 11. Mutation Operations

### 11.1 Mutation Model

The Score IR supports mutations at every level of the hierarchy. Each mutation is an atomic operation that:

1. Modifies the document state.
2. Runs incremental validation on the affected region.
3. Marks affected annotation layers (harmonic, orchestration) as stale.
4. Increments the document version counter.
5. Pushes an inverse operation onto the undo stack.

Mutations are the primary interface through which an agent or user modifies the score. They are defined as operations on the Score IR, not as raw field assignments; this allows the Score IR to maintain invariants.

### 11.2 Event-Level Mutations

| Operation | Parameters | Effect |
|-----------|-----------|--------|
| `InsertNote` | part, bar, voice, offset, note, duration | Inserts a NoteGroup at the specified position; displaces subsequent events |
| `DeleteEvent` | event_id | Removes an event; fills gap with rest |
| `ModifyPitch` | event_id, note_index, new_pitch | Changes the pitch of a note within a NoteGroup |
| `ModifyDuration` | event_id, new_duration | Changes the duration; validates measure fill |
| `ModifyVelocity` | event_id, note_index, new_velocity | Changes the velocity |
| `SetArticulation` | event_id, note_index, articulation | Adds or changes an articulation |
| `SetDynamic` | part_id, position, dynamic_level | Inserts a dynamic marking |
| `InsertHairpin` | part_id, start, end, type, target | Inserts a crescendo/diminuendo |
| `SetTie` | event_id, note_index, tied | Sets or clears a tie forward |
| `TransposeEvent` | event_id, interval | Transposes all notes by a diatonic interval |

### 11.3 Measure-Level Mutations

| Operation | Parameters | Effect |
|-----------|-----------|--------|
| `InsertMeasure` | after_bar, count | Inserts empty measures in all parts; updates SectionMap, TempoMap, etc. |
| `DeleteMeasure` | bar, count | Removes measures from all parts; updates global maps |
| `SetTimeSignature` | bar, time_signature | Updates the TimeSignatureMap; revalidates measure fill for affected bars |
| `SetKeySignature` | position, key_signature | Updates the KeySignatureMap |
| `AddVoice` | part, bar | Adds a new voice to a measure |
| `RemoveVoice` | part, bar, voice_index | Removes a voice (must be empty or contain only rests) |

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
| `CopyRegion` | source_region, target_position | Copies note content to a new location |
| `MoveRegion` | source_region, target_position | Moves note content (copy + delete source) |
| `DeleteRegion` | region | Replaces all content in the region with rests |
| `SetDynamicRegion` | region, dynamic_level | Applies a dynamic to all parts in the region |
| `ScaleVelocityRegion` | region, factor | Multiplies all velocities in the region by a factor |
| `RetrogadeRegion` | region | Reverses the temporal order of events in the region |
| `InvertRegion` | region, axis_pitch | Inverts all pitches about a given axis |
| `AugmentRegion` | region, factor | Multiplies all durations by a rational factor |
| `DiminuteRegion` | region, factor | Divides all durations by a rational factor |

### 11.6 Orchestration Mutations

| Operation | Parameters | Effect |
|-----------|-----------|--------|
| `Reorchestrate` | source_part, target_part, region, role | Moves melodic content from one part to another, updating orchestration annotations |
| `DoubleAtInterval` | source_part, target_part, region, interval | Writes the source part's melody into the target part, transposed |
| `SetTextureRole` | part_id, region, role | Updates the orchestration annotation for a part in a region |
| `ApplyVoiceLeading` | region, constraints | Runs the Theory Spec's voice-leading algorithm on the harmonic content in the region and distributes voices to parts according to their roles |

### 11.7 Undo/Redo

Every mutation records its inverse. The undo stack is a sequence of inverse operations. Undo pops the stack and applies the inverse; redo re-applies the original. The stack depth is unbounded (limited only by memory).

**Group operations**: A sequence of mutations can be grouped as a single undoable unit (e.g., "reorchestrate bars 33–48" might involve dozens of individual mutations, but undoing it reverts all of them at once).

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
| `create_score` | Initialise a new Score IR with metadata, parts, tempo, key, and time signature |
| `set_formal_plan` | Define the SectionMap (e.g., "sonata form in D major, 3 sections") |
| `add_part` | Add an instrument to the score |
| `set_section_harmony` | Write a chord progression for a section |

**Arrangement tools** (mid-level, operating on parts and regions):

| Tool | Description |
|------|-------------|
| `write_melody` | Write a melodic line into a part for a region |
| `write_harmony` | Write chord voicings into one or more parts |
| `reorchestrate` | Move musical material between parts |
| `double_part` | Create a doubling of one part in another |
| `set_dynamics` | Apply dynamic markings to a region |
| `set_articulation` | Apply articulations to notes in a region |

**Detail tools** (low-level, operating on individual events):

| Tool | Description |
|------|-------------|
| `insert_note` | Insert a single note |
| `modify_note` | Change pitch, duration, velocity, or articulation |
| `delete_event` | Remove an event |
| `transpose` | Transpose a note, event, or region |

**Analysis tools** (read-only):

| Tool | Description |
|------|-------------|
| `analyze_harmony` | Get harmonic analysis for a region |
| `get_orchestration` | Get textural role assignments |
| `get_reduction` | Get a piano reduction or short score |
| `validate_score` | Run validation and return diagnostics |
| `get_form_summary` | Get the formal structure overview |

**Compilation tools**:

| Tool | Description |
|------|-------------|
| `compile_to_ableton` | Compile and push to Ableton via LOM bridge |
| `compile_to_midi` | Compile to MIDI file |
| `compile_to_musicxml` | Compile to MusicXML |
| `compile_to_lilypond` | Compile to LilyPond |

### 12.3 Agent Workflow Patterns

The following are reference workflow patterns for different compositional tasks.

**Solo piano piece**:

1. `create_score` with one Piano part.
2. `set_formal_plan` (e.g., ABA ternary).
3. For each section: `set_section_harmony` → `write_melody` → `write_harmony` (inner voices).
4. `set_dynamics` and `set_articulation` for expression.
5. `validate_score`.
6. `compile_to_ableton`.

**Orchestral work**:

1. `create_score` with full orchestral parts (strings, woodwinds, brass, percussion).
2. `set_formal_plan` (sonata form: Exposition, Development, Recapitulation, Coda).
3. Compose the harmonic skeleton: `set_section_harmony` for each section.
4. Compose primary thematic material: `write_melody` into a lead part (e.g., Violin I).
5. Orchestrate: `reorchestrate` and `double_part` to distribute material across the ensemble.
6. Add countermelodies: `write_melody` into secondary parts.
7. Fill harmonic support: `write_harmony` into sustaining instruments.
8. Add rhythmic material: `write_melody` for ostinato or rhythmic patterns.
9. Apply dynamics and articulations: `set_dynamics`, `set_articulation`.
10. Review via `get_reduction` (piano reduction) for harmonic coherence.
11. `validate_score` — resolve any range, doubling, or voice-leading issues.
12. `compile_to_ableton`.
13. Listen, revise at the Score IR level, re-compile.

**Electronic / production track**:

1. `create_score` with synthesiser and drum machine parts.
2. `set_formal_plan` (Intro, Verse, Chorus, Bridge, Drop, Outro).
3. Use `apply_euclidean_rhythm` (existing MCP tool) for rhythmic patterns.
4. `write_melody` for synth leads.
5. `set_section_harmony` for harmonic content.
6. `compile_to_ableton` — the DAW becomes the primary editing environment for sound design, while the Score IR manages structure and arrangement.

---

## 13. Serialisation

### 13.1 On-Disk Format

The Score IR is serialised as a single JSON document (for human readability and tool interoperability) or as a binary format (for performance with large scores). Both formats represent the same logical structure.

**JSON schema**: Follows the type definitions in this specification directly. Field names are snake_case. Enumerations are serialised as strings. Beat values are serialised as `{"n": numerator, "d": denominator}`. SpelledPitch is serialised as `{"letter": "C", "accidental": 0, "octave": 4}`. Ids are serialised as strings.

**Binary format**: A custom compact binary encoding (specification TBD) for scores exceeding 10,000 events, where JSON parsing overhead becomes measurable.

### 13.2 Versioning

The serialisation format includes a schema version number. The Score IR library supports:
- **Forward compatibility**: A newer library can read older format versions by applying default values for newly added fields.
- **Backward compatibility**: An older library can read newer format versions by ignoring unknown fields (within the same major version).

Major version increments indicate breaking changes that require migration.

### 13.3 Invariant Re-Validation on Load

When a Score IR document is deserialised, full validation (§10) runs before the document is made available. This ensures that a document saved by one version of the library and loaded by another is always in a consistent state.

---

## 14. Cross-Document References

### 14.1 Dependencies on Theory Spec

The Score IR depends on the Theory Spec for all musical primitives. Changes to the Theory Spec that affect the types listed in §0.4 require corresponding updates to the Score IR.

The dependency is one-directional: the Theory Spec does not depend on or reference the Score IR. The Theory Spec defines *what music is*; the Score IR defines *what a specific piece of music contains*.

### 14.2 Dependencies on Infrastructure

The Score IR depends on the existing Sunny infrastructure for:

| Component | Usage |
|-----------|-------|
| LOM Bridge | AbletonCompiler sends commands via the LOM bridge |
| OSC Transport | Real-time parameter updates during playback |
| MCP Server | Agent-facing tool interface |
| Transport | Playback scheduling (the Transport reads TickTime events) |

### 14.3 Compilation Pipeline

The full pipeline from agent intent to sounding music:

```
Agent Intent
    ↓ (MCP tool calls)
Score IR (mutations + queries)
    ↓ (compilation)
Rendering Target (Ableton session / MIDI / MusicXML)
    ↓ (playback / engraving)
Sounding Music / Printed Score
    ↓ (agent listening / analysis)
Score IR (revisions via MCP)
```

The Score IR is the single point of truth throughout this cycle. The agent never edits the rendering target directly; it always operates through the Score IR.

---

## 15. Invariant Summary

### 15.1 Structural Invariants

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

### 15.2 Compilation Invariants

11. ScoreTime → AbsoluteBeat is monotonically increasing.
12. AbsoluteBeat → TickTime is monotonically increasing.
13. TickTime rounding error does not exceed 0.5 ticks per bar.
14. For every valid Score IR, each compiler produces exactly one output.
15. Concert pitch storage: all Note pitches are sounding pitch, never written pitch.

### 15.3 Annotation Layer Invariants

16. HarmonicAnnotation entries are non-overlapping and cover the score duration.
17. OrchestrationAnnotation entries for a single part are non-overlapping within that part.
18. Stale annotation regions are tracked and reported by validation.

### 15.4 Mutation Invariants

19. Every mutation has a computable inverse.
20. Applying a mutation followed by its inverse restores the previous document state exactly.
21. The version counter increases monotonically and is never reused.

---

## Appendix A: Standard Instrument Library

Reference definitions for common orchestral instruments.

| Instrument | Class | Transposition | Range (absolute) | Range (comfortable) | Clef | Staves |
|-----------|-------|---------------|-----------------|--------------------|----|--------|
| Piccolo | Woodwinds.Piccolo | +P8 (sounds octave higher) | D4–C8 (written D5–C9) | D5–A7 | Treble | 1 |
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
| **Compilation** | Deterministic transformation of Score IR to a rendering target |
| **Direction** | A zero-duration annotation attached to a time point |
| **Event** | The atomic unit of musical content (note, rest, chord symbol, or direction) |
| **Hairpin** | A gradual dynamic change (crescendo or diminuendo) |
| **HarmonicAnnotation** | A chord analysis entry in the harmonic layer |
| **Measure** | A single bar within a part |
| **MCP** | Model Context Protocol; the JSON-RPC interface for agent interaction |
| **NoteGroup** | One or more simultaneous notes sharing onset and duration |
| **OrchestrationLayer** | Annotations describing the textural role of each part |
| **Part** | A single instrumental line in the score |
| **PartDirective** | A scoped performance instruction for an entire part |
| **RealTime** | Clock time in seconds, derived from AbsoluteBeat and TempoMap |
| **Region** | A bounded span of score time, optionally restricted to a subset of parts |
| **RenderingConfig** | DAW-specific configuration for compiling a part |
| **Score IR** | The intermediate representation; the complete document model |
| **ScoreTime** | The primary temporal coordinate: (bar number, beat within bar) |
| **Section** | A labelled formal unit in the score's structure |
| **SpelledPitch** | A pitch with explicit letter name, accidental, and octave |
| **Stale** | An annotation region whose backing note content has changed since the annotation was computed |
| **TempoMap** | A piecewise function from score time to tempo |
| **TexturalRole** | The function a part serves in the texture (melody, bass, harmonic fill, etc.) |
| **TickTime** | Discretised time in MIDI pulses |
| **TupletContext** | Grouping metadata for tuplet notation |
| **Validation** | Checking the Score IR against its invariants |
| **View** | A derived, read-only projection of the score (piano reduction, short score, etc.) |
| **Voice** | A monophonic stream of events within a measure |

---

*End of specification.*
