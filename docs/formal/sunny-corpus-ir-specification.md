# Sunny Corpus IR — Formal Specification

**Version:** 0.2.0-draft
**Date:** 2026-08-31
**Status:** Normative model with an explicit implemented runtime profile
**Dependencies:** Sunny Engine Formal Specification v0.1.0 (the "Theory Spec"); Sunny Score IR Specification v0.2.0 (the "Score IR Spec")

---

## 0. Preamble

### 0.1 Purpose

This document defines the formal specification for the Sunny Corpus IR, a structured system for ingesting existing music, performing systematic analytical decomposition using the theory engine, extracting compositional patterns and insights, constructing per-composer style profiles, and making the accumulated knowledge queryable by an agent during composition.

The Corpus IR is the fifth and final layer of the Sunny production stack. The four preceding layers — Theory, Score IR, Timbre IR, Mix IR — provide the *executive capacity* to produce music: the vocabulary, the document model, the timbral control, and the production infrastructure. The Corpus IR provides the *deliberative capacity*: the structured understanding of existing music that informs an agent's compositional choices. Without this layer, the agent possesses craft but not education. With it, the agent has access to a systematic, queryable distillation of compositional practice across the repertoire — not as rules to follow, but as patterns to draw upon, adapt, combine, and when the musical context demands it, deliberately violate.

### 0.2 Foundational Principle: Composer Individuality

The Corpus IR does not model genres, periods, or schools as primary organisational units. It models *individual composers*. There is no single "Classical style" that encompasses both Haydn's wit and Mozart's operatic instinct. There is no single "Romantic style" that captures both Chopin's intimate pianism and Wagner's orchestral architecture. There is no single "Impressionist style" that accounts for both Debussy's whole-tone wanderings and Ravel's mechanical precision. Genre and period labels are secondary metadata; the primary analytical unit is the individual compositional voice.

This principle extends beyond the Western classical tradition. In jazz, Monk's angularity and Evans's lyricism are not two instances of a common "jazz piano style"; they are distinct compositional personalities with different harmonic vocabularies, different rhythmic dispositions, different approaches to form. In electronic music, Aphex Twin's algorithmic complexity and Burial's spectral melancholy are not interchangeable instances of "electronic music." Every composer, in every tradition, makes particular choices from the available vocabulary; those choices constitute a style, and that style is individual.

The Corpus IR therefore constructs *per-composer style profiles*. Cross-composer analysis (comparing Debussy's harmonic vocabulary with Ravel's, or tracking the evolution of sonata form from Haydn through Brahms) is supported as a secondary operation over individual profiles, not as a substitute for them.

### 0.3 Relationship to Other Layers

The Corpus IR is a *read-side* system. It consumes existing music (via ingestion) and produces structured knowledge (via analysis). The four production layers are *write-side* systems. They consume compositional decisions and produce music.

The interface between the two sides is the agent. During composition, the agent queries the Corpus IR for insights, patterns, and style characteristics, then uses the production layers to realise its decisions. The Corpus IR never writes directly to the Score IR; it informs the agent, and the agent writes.

```
Existing Music (MIDI or uncompressed MusicXML)
    ↓ [Ingestion Pipeline]
Corpus IR (structured analytical knowledge)
    ↓ [Agent queries]
Agent (deliberation, decision-making)
    ↓ [MCP tool calls]
Score IR + Timbre IR + Mix IR (production)
    ↓ [Compilation]
New Music
```

### 0.4 Notation Conventions

Inherits all conventions from preceding specifications. Additional conventions:

- Runtime statistical distributions are finite ordered maps of counts or normalised frequencies;
  the current model contains no kernel-density or fitted-parametric value type.
- Frequencies of occurrence are given as counts or proportions (0.0–1.0) relative to the analytical unit (per piece, per phrase, per harmonic change).
- Confidence intervals and sample sizes are reported alongside statistical summaries to prevent overconfident generalisation from small corpora.

### 0.5 Implemented Runtime Profile

The current runtime provides an in-memory CorpusDatabase, versioned JSON serialisation, MIDI and uncompressed MusicXML ingestion, analytical and lifecycle workflows, synchronised composer/period profiles, comparison, and 22 MCP tools. It does not ingest audio, MEI, ABC, Humdrum, or compressed `.mxl`; it has no SQLite index or automatic persistence. Those formats and storage extensions are not accepted by the current MCP surface.

---

## 1. Ingestion Pipeline

### 1.1 Overview

The ingestion pipeline converts external music representations into Score IR documents suitable for analysis. This is a one-way, lossy transformation: the external format is consumed and converted; the resulting Score IR is an analytical representation, not a publication-quality score. The goal is to capture the compositional substance — harmonic content, melodic material, rhythmic structure, formal proportions, dynamic shape — with sufficient fidelity for meaningful statistical and structural analysis.

### 1.2 Runtime-Supported Input Formats

| Format | Fidelity | Primary Use Case |
|--------|----------|-----------------|
| MIDI (SMF Type 0/1) | Medium | Piano reductions, keyboard works, sequenced arrangements |
| MusicXML (uncompressed text) | Structural subset | Parts, canonical measures, numeric voices, notes/rests, chord ownership, typed single-chord harmony points, integer durations and spellings, globally coherent traditional key/metre maps |

MEI, ABC, Humdrum **kern, audio, and compressed MusicXML (`.mxl`) are outside the implemented ingestion boundary and are rejected rather than guessed.

### 1.3 MIDI Ingestion

MIDI is the most abundant format for existing music in digital form. Piano reductions of orchestral works, keyboard compositions, and sequenced arrangements are widely available as MIDI files. MIDI ingestion is therefore the most critical pipeline.

The MIDI ingestion result is loss-accountable for valid event classes outside its analytical
Score projection. Unknown meta events, SysEx, polyphonic aftertouch, channel pressure, and pitch
bend are counted by the SMF parser and copied into `IngestionConfidence.manual_corrections`; their
absence from Score IR is never represented as a lossless import.

The four-field SMF Time Signature payload is also handled as a complete boundary. Same-tick events
must agree in numerator, denominator, metronome clocks per click, and notated 32nds per MIDI
quarter. Score uses the ordinary `bb=8` timing algebra; another value is rejected because it would
change notation-to-clock scaling. `cc` is metronome intent rather than an ordered metre partition.
Ingestion therefore applies the deterministic Score grouping for `nn/dd` and records every
nonmatching click interval under `midi.time_signature_metronome_clicks` instead of inventing
grouping semantics.

Channel state has a narrower reversible subset. The ingester preserves a sole source note channel
as the generated Part's 1-based rendering channel. On that channel, completed binary Damper
(CC64) and Soft Pedal (CC67) state-change pairs become `SustainingPedal` and `UnaCorda`
`PartDirective` spans. MIDI defines 0…63 as off and 64…127 as on; threshold-equivalent values are
semantically imported but recorded under `midi.pedal_switch_values` because Score recompilation
uses canonical 0/127 bytes. A same-tick off/on change, repeated state message, unmatched endpoint,
controller on another channel, or any other CC remains in `midi.control_change_events`. Program
changes remain in `midi.program_change_events`: an `ArticulationMapping` describes how a known
Score articulation compiles and is not an arbitrary time-positioned source program carrier.
When notes occupy multiple channels, no channel-local controller is globalised into the one-Part
analytical reduction and `midi.note_channel_ownership` records that collapse. Type-1 track
topology is likewise recorded under `midi.track_topology` even when some declared tracks are empty.

**Challenges and solutions**:

| Challenge | Description | Solution |
|-----------|-------------|----------|
| No enharmonic spelling | MIDI stores note numbers (0–127), not spelled pitches | Spelling inference from key context and melodic interval patterns [H] |
| No explicit key signatures | Key must be inferred from pitch content | Use every valid SMF key event; infer a major/minor context only before the first event or when none exists [H] |
| No barline positions | Only tick offsets from the start of the file | Derive the complete bar lattice from ordered, complete SMF time-signature events under the admitted `bb=8` scale; use 4/4 before the first event or when absent |
| No formal structure | No section markers, phrase boundaries, or labels | Not inferred by the current ingester; analysis may leave the section plan empty |
| No dynamics (often) | Velocity is present but may not reflect compositional dynamics | Velocity treated as relative dynamics; absolute dynamic levels are not inferred |
| Quantisation noise | Performed MIDI has timing imprecision | Quantise onset and duration independently by exact rational arithmetic and retain separate RMS losses |
| No articulation labels | Duration and velocity encode articulation implicitly | Preserve duration/velocity only; do not invent articulation labels |
| Channel-local state | CC and program events are ordered channel messages, while the compact ingester creates one analytical Part | Preserve the sole note channel and completed CC64/CC67 switch spans only; account for every other event and ownership collapse explicitly |

#### 1.3.1 MIDI Ingestion Pipeline

**Stage 1: Parse.** Read the MIDI file. Extract declared tracks, tempo, time/key signatures,
note-on/note-off pairs, control changes, and program changes with exact ticks, channels, source
tracks, source order, attack velocity, and retained release velocity.

**Stage 2: Quantise.** Let the positive integer grid parameter `g` denote `g` equal divisions of
one whole note (default `g = 16`, a sixteenth note). For each non-negative onset and positive
duration `x`, compute `xg` with checked rational arithmetic, round its non-negative integer quotient
to the nearest integer with an exact half-grid tie rounded forward, and return that integer divided
by `g`. A positive duration that would round to zero is raised to exactly `1/g`; onsets and
durations otherwise use the same rule. Arithmetic failure rejects ingestion. Floating point is not
used to choose the grid point. It is used only after the exact difference is known to report the
two evidence metrics

`onset_rms = sqrt(sum((onset_source - onset_grid)^2) / note_count)` and
`duration_rms = sqrt(sum((duration_source - duration_grid)^2) / note_count)`,

both in whole-note units and both zero for an empty note sequence. The grid is an internal Corpus
analysis policy. It is not Live Clip launch quantisation, a Live groove, Max scheduler resolution,
or a Max object's playback-quantisation policy.

**Stage 3: Assign measures.** Sort and reconcile time-signature events, require every actual change to lie on a boundary in the preceding metre, and derive the complete variable-metre bar lattice. Use 4/4 before the first event or when absent. An off-boundary change is rejected because `TimeSignatureMap` is bar-indexed.

**Stage 4: Assign voices and routing evidence.** Group simultaneous onsets, order each group from
highest to lowest pitch, and allocate the first lane whose preceding note has ended. Create every
required Score voice before insertion. Notes crossing a barline are split into destination-first,
same-pitch tie chains so every intermediate mutation remains valid and MIDI recompilation recovers
one sounding note. A sole source note channel becomes the Part rendering channel. Multiple
channels and Type-1 track topology remain correction evidence rather than inferred Parts. The
confidence record reports how often polyphonic onsets required the assignment heuristic.

**Stage 5: Resolve keys.** Preserve ordered SMF key-signature events as exact major/minor fifths-and-tonic contexts. Before the first explicit event, or when none exists, estimate a major or minor key by correlating the full pitch-class histogram against the 24 rotated Krumhansl-Kessler profiles. Windowed modulation detection is not part of the current ingester.

**Stage 6: Spell pitches.** Convert MIDI note numbers to SpelledPitch values using the active explicit-or-inferred key's line-of-fifths context and deterministic default spelling.

**Voice allocation.** Group quantised notes by onset. Within an onset, order notes by descending
pitch and then retained source ordinal, and assign each to the first voice whose prior note has
ended. An onset is *complex* when it contains more than one note or when any prior voice is still
sounding. For `c` complex onsets among `n>0` onsets, record
`clamp(1 - c/n, 0.3, 1.0)` and a `midi.voice_separation` correction carrying the exact `c/n`
counts whenever `c>0`. This is deterministic uncertainty evidence, not recovered SMF voice
identity. Release velocity and source ordinal travel with the selected note; they are never
re-associated by equal-pitch sort instability.

**Stage 7: Construct Score IR.** Assemble the processed data into Score → Parts → Measures → Voices
→ Events. Attach exact immediate tempo, time-signature, and explicit/inferred key maps plus every
admissible completed CC64/CC67 Part span. Conflicting same-tick metadata, a target-invalid rate,
voice-capacity overflow, or any failed structural insertion fails ingestion. Every valid event not
admitted by this Score projection receives named correction evidence. Mark the analytical
confidence for each inferred property.

**Stage 8: Analyse.** Revalidate the populated Score, run the implemented Score analysis, and store its result with `analysis_complete = true`.

#### 1.3.2 Ingestion Confidence

Each ingested Score IR carries an *IngestionConfidence* record:

| Field | Type | Description |
|-------|------|-------------|
| `key_confidence` | `f32` | Confidence in key estimation (0.0–1.0) |
| `metre_confidence` | `f32` | Confidence in metre/barline placement |
| `spelling_confidence` | `f32` | Confidence in enharmonic spelling |
| `voice_separation_confidence` | `f32` | Deterministic overlap-aware confidence in voice assignment |
| `quantisation_residual` | `f32` | RMS onset error introduced by quantisation (in whole-note units) |
| `duration_quantisation_residual` | `f32` | RMS duration error introduced by quantisation (in whole-note units) |
| `source_format` | `String` | Original format |
| `manual_corrections` | `Vec<ManualCorrection>` | Human corrections and machine-recorded source facts that the analytical Score projection did not preserve exactly |

For MusicXML ingestion, metre, spelling, and voice-separation confidence are currently recorded as
1.0. Key confidence is 1.0 when an explicit key is found; otherwise it is the global pitch-profile
estimate. Both quantisation residuals are zero because this path retains the admitted exact
MusicXML durations rather than applying Stage 2. These values describe the implemented structural
extraction, not preservation of every notation field.

### 1.4 MusicXML Ingestion

The current ingester accepts uncompressed `<score-partwise>` MusicXML and maps a fail-closed structural subset into Score IR: declared parts, canonical 1…N measure numbering, numeric voices, sequential pitched notes/rests, equal-duration chord ownership, typed single-chord harmony points, integer divisions/durations/alterations, spelled pitches, and every globally coherent bar-level traditional key and time-signature change. A harmony point retains its exact cursor-relative measure offset and maps root-or-numeral, explicit numeral key, kind, bass, inversion, and ordered add/alter/subtract degrees into a `ChordSymbolEvent` in the primary voice. The numeral root spelling is derived by the same core authority used by Score validation. All parts must have the same measure count and effective key/metre context at each bar; divergent part-local contexts are rejected because this Corpus profile does not project them into `Measure.local_key` or `Measure.local_time`.

The low-level reader rejects cursor `backup`/`forward`, mid-measure attributes, decimal timing and microtonal alterations, additive/composite metre syntax, string voice labels, non-numeric measure labels, multi-staff/transposition state, grace/cue/unpitched notes, and malformed or missing numeric fields. Harmony `function`, stacked chords, frames, fractional alterations, non-primary staff assignment, and unretained semantic/display attributes are likewise rejected. These are legal in broader MusicXML but absent from the compact type algebra. Traditional key ingestion retains the exact signed `<fifths>` value and every registered MusicXML mode (major, minor, the seven church modes, Ionian/Aeolian aliases); the tonic is derived with that mode's line-of-fifths adjustment. MusicXML's alternative non-traditional `key-step`/`key-alter` branch and interval-free `none` mode are rejected, never reinterpreted as C major. Every note insertion must preserve exact measure tiling, and every harmony offset must lie strictly inside its declared measure, after which the populated Score is revalidated and analysed.

This remains intentionally bounded corpus ingestion, not a total inverse of the MusicXML compiler. The typed harmony subset is invertible; free-form `kind` display text is retained only as one opaque extension where unambiguous and is never parsed as a second harmony language. Imported parts currently use Piano/Treble defaults, tempo defaults to 120 BPM, note velocity defaults to 80, and notation details such as dynamics, articulations, lyrics, slurs, ties, clef changes, and later map changes are not copied into the IngestedWork. The confidence record must not be interpreted as publication-quality round-trip fidelity.

### 1.5 Ingested Work

**Definition 1.5.1**. An *IngestedWork* is the complete analytical record for a single piece of music.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<IngestedWork>` | Unique identifier |
| `metadata` | `WorkMetadata` | Descriptive metadata |
| `score` | `Score` | The Score IR representation |
| `ingestion_confidence` | `IngestionConfidence` | Quality metrics for the ingestion |
| `analysis` | `WorkAnalysis` | Full analytical decomposition (§2) |

### 1.6 WorkMetadata

| Field | Type | Description |
|-------|------|-------------|
| `title` | `String` | Work title |
| `composer` | `ComposerRef` | Reference to the composer profile |
| `opus` | `Option<String>` | Opus or catalogue number |
| `year_composed` | `Option<u16>` | Year of composition (approximate) |
| `period` | `Option<String>` | Stylistic period (for secondary classification only) |
| `genre` | `Option<String>` | Genre label (secondary) |
| `instrumentation` | `String` | Original instrumentation |
| `source_format` | `String` | Format of the ingested file |
| `source_description` | `Option<String>` | e.g., "Piano reduction by the composer", "MIDI arrangement by [username]", "Urtext edition" |
| `is_reduction` | `bool` | Whether this is a reduction of a larger work |
| `original_instrumentation` | `Option<String>` | If a reduction, the original instrumentation |
| `movement` | `Option<String>` | Movement number/title within a larger work |
| `tags` | `Vec<String>` | Free-form tags |

---

## 2. Analytical Decomposition

### 2.1 Overview

Once a work has been ingested into Score IR form, the Corpus IR performs a systematic analytical decomposition using the theory engine. The result is a *WorkAnalysis*: the complete runtime carrier for the supported evidence domains. “Complete” means field-complete with respect to this declared carrier; it does not mean that every musicological property is automatically inferred, or even represented. Section 3.15 distinguishes automatically analysed evidence, caller-supplied evidence, and properties that are not currently derivable.

The decomposition is organised by analytical domain. Each domain addresses a different dimension of compositional practice.

### 2.2 WorkAnalysis

**Definition 2.2.1**. A *WorkAnalysis* is the complete analytical record for a single work.

| Field | Type | Description |
|-------|------|-------------|
| `harmonic_analysis` | `HarmonicAnalysisRecord` | Chord-by-chord harmonic analysis |
| `melodic_analysis` | `MelodicAnalysisRecord` | Melodic properties of each voice |
| `rhythmic_analysis` | `RhythmicAnalysisRecord` | Rhythmic properties |
| `formal_analysis` | `FormalAnalysisRecord` | Large-scale structure |
| `voice_leading_analysis` | `VoiceLeadingAnalysisRecord` | Voice-leading patterns and practices |
| `textural_analysis` | `TexturalAnalysisRecord` | Density, register, spacing |
| `dynamic_analysis` | `DynamicAnalysisRecord` | Dynamic shape and usage |
| `orchestration_analysis` | `Option<OrchestrationAnalysisRecord>` | Present only for orchestral scores |
| `motivic_analysis` | `MotivicAnalysisRecord` | Motivic material and transformations |

### 2.3 Harmonic Analysis Record

**Definition 2.3.1** [H]. The *HarmonicAnalysisRecord* contains a chord-by-chord analysis of the entire work.

| Field | Type | Description |
|-------|------|-------------|
| `chord_vocabulary` | `Map<String, u32>` | Frequency count by Roman-numeral/chord label |
| `progression_inventory` | `Vec<ProgressionPattern>` | Catalogued chord progressions |
| `modulation_inventory` | `Vec<ModulationEvent>` | Key changes with technique classification |
| `harmonic_rhythm` | `HarmonicRhythmProfile` | Distribution of harmonic change rates |
| `cadence_inventory` | `Vec<CadenceEvent>` | All cadences with type and position |
| `chromatic_techniques` | `Vec<ChromaticEvent>` | Secondary dominants, augmented sixths, Neapolitans, common-tone diminished, etc. |
| `tonicisation_frequency` | `Map<u8, u32>` | How often each encoded scale degree is tonicised |
| `tonal_plan` | `TonalPlan` | Sequence of key areas with durations |

#### 2.3.1 ProgressionPattern

| Field | Type | Description |
|-------|------|-------------|
| `roman_numerals` | `Vec<String>` | Sequence of Roman numerals (e.g., ["I", "IV", "V7", "I"]) |
| `length` | `u8` | Number of chords in the pattern |
| `occurrences` | `Vec<ScoreTime>` | Where this pattern appears in the score |
| `key_context` | `String` | Simplified key label in which this pattern occurs |

#### 2.3.2 ModulationEvent

| Field | Type | Description |
|-------|------|-------------|
| `position` | `ScoreTime` | Where the modulation occurs |
| `from_key` | `String` | Simplified key label before modulation |
| `to_key` | `String` | Simplified key label after modulation |
| `technique` | `ModulationTechnique` | How the modulation is achieved |
| `pivot_chord` | `Option<String>` | The pivot chord, if applicable |

**ModulationTechnique**: `PivotChord`, `ChromaticMediant`, `Enharmonic`, `DirectModulation`, `Sequential`, `CommonTone`, `Abrupt`.

#### 2.3.3 HarmonicRhythmProfile

| Field | Type | Description |
|-------|------|-------------|
| `changes_per_bar` | `Vec<f32>` | Harmonic change rate for each bar |
| `mean_rate` | `f32` | Average changes per bar across the work |
| `variance` | `f32` | Variance of harmonic rhythm |
| `rate_by_section` | `Map<String, f32>` | Mean rate per formal section |

#### 2.3.4 CadenceEvent

| Field | Type | Description |
|-------|------|-------------|
| `position` | `ScoreTime` | Where the cadence falls |
| `type` | `CadenceType` | PAC, IAC, HC, DC, PC, Phrygian |
| `approach` | `Vec<String>` | Roman numerals of the cadential approach (last 2–4 chords) |
| `section_context` | `String` | Which formal section this cadence occurs in |
| `is_structural` | `bool` | Whether this is a phrase-level or section-level cadence |

### 2.4 Melodic Analysis Record

**Definition 2.4.1** [H/C]. The *MelodicAnalysisRecord* analyses the melodic properties of each voice in the work.

| Field | Type | Description |
|-------|------|-------------|
| `per_voice_analysis` | `Vec<VoiceMelodicAnalysis>` | Analysis per voice/part |
| `primary_melody_voice` | `Id<Part>` | Which voice carries the primary melody most often |
| `thematic_material` | `Vec<ThematicUnit>` | Identified themes and motifs |

#### 2.4.1 VoiceMelodicAnalysis

| Field | Type | Description |
|-------|------|-------------|
| `part_id` | `Id<Part>` | Which voice |
| `note_count` | `u32` | Number of extracted melody notes; zero distinguishes an empty analytical sentinel from a measured voice |
| `range_low`, `range_high` | `i8, i8` | Lowest and highest MIDI-note values in the compact runtime carrier |
| `tessitura_low`, `tessitura_high` | `i8, i8` | Compact MIDI-note tessitura bounds |
| `interval_distribution` | `Map<Interval, u32>` | Frequency of each melodic interval |
| `contour_inventory` | `Vec<ContourSegment>` | Catalogued melodic contour shapes |
| `scale_degree_distribution` | `Map<u8, u32>` | Frequency of each scale degree 1–7 of the opening key; bucket 0 counts chromatic (non-diatonic) notes, so the histogram sums to the note count |
| `leap_resolution_rate` | `f32` | Proportion of leaps (> M2) followed by stepwise motion |
| `conjunct_proportion` | `f32` | Proportion of stepwise intervals |
| `longest_ascending_run` | `u8` | Longest consecutive ascending stepwise motion |
| `longest_descending_run` | `u8` | Longest consecutive descending stepwise motion |
| `chromaticism_rate` | `f32` | Proportion of non-diatonic pitches |

A same-pitch tie chain within one voice is one sounding note. Its continuations extend the attacked
note's duration but add no melody note, interval, or onset; a rest breaks every pending tie. The
same folding governs the rhythmic record's onsets, durations, and syncopation index (§2.5).

#### 2.4.2 ContourSegment

| Field | Type | Description |
|-------|------|-------------|
| `start` | `ScoreTime` | Beginning of the contour segment |
| `end` | `ScoreTime` | End of the contour segment |
| `shape` | `ContourShape` | Classification |
| `pitch_range` | `Interval` | Span of the segment |
| `duration_beats` | `f32` | Temporal span in beats |
| `peak_position` | `f32` | Relative position of the highest pitch (0.0 = start, 1.0 = end) |
| `nadir_position` | `f32` | Relative position of the lowest pitch |

**ContourShape**: `Ascending`, `Descending`, `Arch` (ascends then descends), `InvertedArch`, `Stationary`, `Oscillating`, `AscendingArch`, `DescendingArch`, `Complex`.

#### 2.4.3 ThematicUnit

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<ThematicUnit>` | Unique identifier |
| `label` | `String` | e.g., "First theme", "Second theme", "Closing theme", "Motto" |
| `intervals` | `Vec<Interval>` | Interval sequence (more stable across transpositions) |
| `rhythm` | `Vec<f32>` | Duration sequence in beats |
| `contour` | `Vec<i8>` | Contour reduction (−1, 0, +1 for descending, same, ascending) |
| `occurrences` | `Vec<ThematicOccurrence>` | Where this theme appears |

**ThematicOccurrence**:

| Field | Type | Description |
|-------|------|-------------|
| `position` | `ScoreTime` | Location |
| `part_id` | `Id<Part>` | Which voice carries it |
| `transformation` | `ThematicTransformation` | How it relates to the original statement |
| `key` | `String` | Simplified key label of this statement |

**ThematicTransformation**: `Original`, `TransposedExact`, `TransposedTonal`, `Inverted`, `Retrograde`, `RetrogradeInversion`, `Augmented`, `Diminished`, `Fragmented`, `Extended`, `SequentialRepetition`, `Ornamented`, `Simplified`, `Developed`.

### 2.5 Rhythmic Analysis Record

**Definition 2.5.1** [C]. The *RhythmicAnalysisRecord* analyses rhythmic properties.

| Field | Type | Description |
|-------|------|-------------|
| `duration_distribution` | `Map<Beat, u32>` | Frequency of each note duration |
| `metre_distribution` | `Map<String, u32>` | Bars by canonical metre label; conventional simple/compound metres use `N/D`, explicit additive groupings use `g+g…/D` |
| `onset_density` | `Vec<f32>` | Note onsets per beat, measured per bar |
| `syncopation_index` | `f32` | Degree of syncopation (Longuet-Higgins/Lee or Witek metric) |
| `rhythmic_motifs` | `Vec<RhythmicMotif>` | Recurring rhythmic patterns |
| `tempo_profile` | `Vec<(ScoreTime, f32)>` | Tempo over the course of the work |
| `rubato_degree` | `f32` | Variability of tempo (0 = metronomic, higher = more rubato) |
| `metrical_complexity` | `f32` | Frequency of time signature changes, asymmetric metres, hemiola |
| `rest_proportion` | `f32` | Proportion of total duration occupied by rests |
| `note_density_by_section` | `Map<String, f32>` | Average note density per formal section |

**RhythmicMotif**:

| Field | Type | Description |
|-------|------|-------------|
| `durations` | `Vec<Beat>` | Sequence of durations |
| `occurrences` | `u32` | Count |
| `positions` | `Vec<ScoreTime>` | Where it appears |

### 2.6 Formal Analysis Record

**Definition 2.6.1** [H]. The *FormalAnalysisRecord* analyses the large-scale structure of the work.

| Field | Type | Description |
|-------|------|-------------|
| `section_plan` | `Vec<FormalSection>` | Ordered sequence of formal sections |
| `form_type` | `FormClassification` | Classification of the overall form |
| `total_duration_bars` | `u32` | Total number of bars |
| `proportions` | `Vec<SectionProportion>` | Relative sizes of sections |
| `tonal_plan` | `TonalPlan` | Key areas associated with formal sections |
| `thematic_assignment` | `Map<String, Vec<Id<ThematicUnit>>>` | Which themes appear in which sections |
| `symmetry_analysis` | `Option<SymmetryAnalysis>` | Arch form, palindrome, or other symmetry properties |

**FormalSection**:

| Field | Type | Description |
|-------|------|-------------|
| `label` | `String` | Section label |
| `start_bar` | `u32` | Starting bar |
| `end_bar` | `u32` | Ending bar |
| `length_bars` | `u32` | Duration in bars |
| `key` | `KeySignature` | Primary key of the section |
| `tempo` | `f32` | Primary tempo |
| `character` | `Option<String>` | Descriptive character (e.g., "lyrical", "agitated", "triumphant") |
| `subsections` | `Vec<FormalSection>` | Nested structure |

**FormClassification**: `SonataAllegro`, `SonataRondo`, `Rondo`, `BinarySimple`, `BinaryRounded`, `Ternary`, `ThroughComposed`, `ThemeAndVariations`, `Fugue`, `Strophic`, `VerseChorus`, `AABA`, `TwelveBarBlues`, `Ritornello`, `Passacaglia`, `Chaconne`, `Concerto`, `Fantasia`, `Prelude`, `Suite`, `Other { description: String }`.

**SectionProportion**:

| Field | Type | Description |
|-------|------|-------------|
| `label` | `String` | Section label |
| `proportion` | `f32` | Section length as proportion of total (0.0–1.0) |
| `golden_ratio_proximity` | `f32` | How close the cumulative proportion at this section boundary is to the golden ratio (0.618) |

**TonalPlan**:

| Field | Type | Description |
|-------|------|-------------|
| `key_sequence` | `Vec<(String, String, u32)>` | Ordered `(key label, chromatic relationship to opening tonic, start bar)` entries |
| `key_area_count` | `u32` | Number of distinct key areas visited |
| `most_distant_key` | `String` | First key at the greatest pitch-class distance from the opening tonic on the circle of fifths |
| `tonic_return_bar` | `Option<u32>` | Bar at which the tonic key returns for the final time |

### 2.7 Voice-Leading Analysis Record

**Definition 2.7.1** [C/H]. The *VoiceLeadingAnalysisRecord* catalogues voice-leading practices.

| Field | Type | Description |
|-------|------|-------------|
| `parallel_fifths_count` | `u32` | Number of parallel perfect fifths |
| `parallel_octaves_count` | `u32` | Number of parallel perfect octaves |
| `contrary_motion_proportion` | `f32` | Proportion of voice pairs moving in contrary motion |
| `oblique_motion_proportion` | `f32` | Proportion with one voice stationary |
| `similar_motion_proportion` | `f32` | Proportion moving in the same direction (not parallel) |
| `parallel_motion_proportion` | `f32` | Proportion moving in parallel |
| `voice_crossing_count` | `u32` | Number of voice crossings |
| `average_voice_independence` | `f32` | Metric of melodic independence across voices |
| `common_tone_retention_rate` | `f32` | How often common tones are retained between successive chords |
| `resolution_patterns` | `Vec<ResolutionPattern>` | How tendency tones resolve |
| `spacing_distribution` | `Map<Interval, u32>` | Distribution of vertical intervals between adjacent voices |

**ResolutionPattern**:

| Field | Type | Description |
|-------|------|-------------|
| `tendency_tone` | `String` | e.g., "leading tone", "chordal seventh", "augmented sixth" |
| `resolution` | `String` | e.g., "resolves up by step", "resolves down by step", "unresolved" |
| `frequency` | `u32` | Count |
| `proportion_resolved` | `f32` | Proportion that resolve according to convention |

### 2.8 Textural Analysis Record

**Definition 2.8.1** [C]. The *TexturalAnalysisRecord* analyses the density and register profile of the work.

| Field | Type | Description |
|-------|------|-------------|
| `density_curve` | `Vec<(ScoreTime, u8)>` | Number of simultaneous voices at each time point |
| `average_density` | `f32` | Mean number of simultaneous voices |
| `density_by_section` | `Map<String, f32>` | Average density per formal section |
| `average_register_span` | `f32` | Mean vertical span in semitones |
| `spacing_profile` | `SpacingProfile` | How voices are distributed across the register |
| `texture_type_proportions` | `Map<String, f32>` | Proportion of the work in each texture type (monophonic, homophonic, polyphonic, etc.) |

**SpacingProfile**:

| Field | Type | Description |
|-------|------|-------------|
| `close_proportion` | `f32` | Proportion with all voices within an octave |
| `open_proportion` | `f32` | Proportion with voices spanning more than two octaves |
| `gap_distribution` | `Map<Interval, u32>` | Distribution of inter-voice gaps |

### 2.9 Dynamic Analysis Record

**Definition 2.9.1** [C]. The *DynamicAnalysisRecord* analyses dynamic usage.

| Field | Type | Description |
|-------|------|-------------|
| `dynamic_range` | `(DynamicLevel, DynamicLevel)` | Softest and loudest markings used |
| `dynamic_distribution` | `Map<DynamicLevel, u32>` | Frequency of each dynamic level |
| `hairpin_count` | `u32` | Number of crescendo/diminuendo markings |
| `dynamic_change_rate` | `f32` | Dynamic changes per bar (average) |
| `dynamic_shape` | `Vec<(ScoreTime, f32)>` | Normalised dynamic level over the course of the work |
| `climax_position` | `f32` | Relative position of the loudest passage (0.0–1.0 through the work) |
| `subito_dynamics_count` | `u32` | Number of sudden dynamic changes (fp, sfz, etc.) |
| `dynamic_by_section` | `Map<String, (DynamicLevel, DynamicLevel)>` | Dynamic range per formal section |

### 2.10 Orchestration Analysis Record

**Definition 2.10.1** [H]. Present only for multi-instrument scores. The *OrchestrationAnalysisRecord* analyses how instruments are deployed.

| Field | Type | Description |
|-------|------|-------------|
| `instrument_usage` | `Map<String, f32>` | Proportion of the work each instrument plays (non-tacet) |
| `instrument_combinations` | `Vec<InstrumentCombination>` | Catalogued combinations of instruments |
| `doubling_patterns` | `Vec<DoublingPattern>` | Common doublings |
| `melody_carrier_distribution` | `Map<String, f32>` | Proportion of melody carried by each instrument |
| `orchestral_crescendo_patterns` | `Vec<OrchestraCrescendoPattern>` | How orchestral build-ups are constructed |
| `density_orchestration_correlation` | `f32` | Correlation between number of active instruments and dynamic level |

**InstrumentCombination**:

| Field | Type | Description |
|-------|------|-------------|
| `instruments` | `Vec<String>` | Set of instruments sounding together |
| `frequency` | `u32` | Number of occurrences |
| `typical_context` | `String` | e.g., "Flute + Violin I unison at piano", "Full brass tutti at fortissimo" |
| `interval_relationship` | `Option<Interval>` | If a consistent doubling interval exists |

**DoublingPattern**:

| Field | Type | Description |
|-------|------|-------------|
| `source_instrument` | `String` | Which instrument is doubled |
| `doubling_instrument` | `String` | Which instrument provides the doubling |
| `interval` | `Interval` | Doubling interval (unison, octave, etc.) |
| `frequency` | `u32` | Count |

**OrchestraCrescendoPattern**: How a composer builds orchestral intensity over a passage:

| Field | Type | Description |
|-------|------|-------------|
| `start` | `ScoreTime` | Beginning of the build |
| `end` | `ScoreTime` | Peak |
| `instrument_entry_order` | `Vec<(String, ScoreTime)>` | Order in which instruments enter |
| `register_expansion` | `bool` | Whether the register expands during the build |

### 2.11 Motivic Analysis Record

**Definition 2.11.1** [H]. The *MotivicAnalysisRecord* tracks how thematic and motivic material is used throughout the work.

| Field | Type | Description |
|-------|------|-------------|
| `thematic_units` | `Vec<ThematicUnit>` | All identified themes and motifs (from §2.4.3) |
| `transformation_inventory` | `Vec<TransformationEvent>` | Every thematic transformation with position |
| `developmental_techniques` | `Vec<DevelopmentalTechnique>` | Techniques used to develop material |
| `thematic_density` | `f32` | Proportion of the work derived from identified thematic material |
| `thematic_economy` | `f32` | Ratio of total musical material to distinct thematic sources |

**TransformationEvent**:

| Field | Type | Description |
|-------|------|-------------|
| `source_theme` | `Id<ThematicUnit>` | Original theme |
| `position` | `ScoreTime` | Where the transformation occurs |
| `transformation` | `ThematicTransformation` | Type of transformation |
| `context` | `String` | Formal section and harmonic context |

**DevelopmentalTechnique**: `Fragmentation`, `Sequence`, `Inversion`, `Augmentation`, `Diminution`, `Stretto`, `Combination` (combining two themes), `Liquidation`, `Reharmonisation`, `RegisterTransfer`, `Timbral` (same material, different instrument).

---

## 3. Style Profile

### 3.1 Overview

A *StyleProfile* aggregates the analytical data from all works by a single composer into a statistical and structural characterisation of that composer's practice. The profile captures *what this composer tends to do* — not as prescriptive rules, but as distributions, preferences, tendencies, and distinctive signatures.

A StyleProfile is built incrementally: each new ingested work contributes additional data points. The profile becomes more representative as the corpus grows, and the system tracks sample sizes so that the agent can assess the reliability of any particular observation.

### 3.2 ComposerProfile

**Definition 3.2.1**. The *ComposerProfile* is the root object for a composer's analytical record.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<ComposerProfile>` | Unique identifier |
| `name` | `String` | Composer name |
| `birth_year` | `Option<u16>` | Year of birth |
| `death_year` | `Option<u16>` | Year of death (if applicable) |
| `active_period` | `Option<(u16, u16)>` | Period of compositional activity |
| `tradition` | `Option<String>` | Musical tradition (e.g., "Western classical", "Jazz", "Electronic") |
| `works` | `Vec<Id<IngestedWork>>` | All ingested works |
| `style_profile` | `StyleProfile` | Aggregated analytical profile |
| `period_profiles` | `Vec<PeriodProfile>` | Sub-profiles for distinct compositional periods |
| `tags` | `Vec<String>` | Secondary classification tags |

### 3.3 PeriodProfile

**Definition 3.3.1**. Some composers undergo significant stylistic evolution. A *PeriodProfile* captures the style of a defined sub-period within a composer's output.

| Field | Type | Description |
|-------|------|-------------|
| `label` | `String` | Period name (e.g., "Early", "Middle", "Late"; or "Viennese period", "Paris period") |
| `year_range` | `(u16, u16)` | Approximate date range |
| `works` | `Vec<Id<IngestedWork>>` | Works assigned to this period |
| `profile` | `StyleProfile` | Aggregated profile for this period |

This allows the system to distinguish, for example, early Beethoven (op. 1–20, classical-period practice) from late Beethoven (op. 106–135, boundary-dissolving formal experimentation). The composer's top-level StyleProfile aggregates across all periods; the PeriodProfiles provide resolution.

### 3.4 StyleProfile

**Definition 3.4.1**. The *StyleProfile* is the aggregate characterisation of a composer's (or a period's) practice.

| Field | Type | Description |
|-------|------|-------------|
| `harmonic_profile` | `HarmonicStyleProfile` | Harmonic tendencies |
| `melodic_profile` | `MelodicStyleProfile` | Melodic tendencies |
| `rhythmic_profile` | `RhythmicStyleProfile` | Rhythmic tendencies |
| `formal_profile` | `FormalStyleProfile` | Formal preferences |
| `voice_leading_profile` | `VoiceLeadingStyleProfile` | Voice-leading practices |
| `textural_profile` | `TexturalStyleProfile` | Textural preferences |
| `dynamic_profile` | `DynamicStyleProfile` | Dynamic usage |
| `orchestration_profile` | `Option<OrchestrationStyleProfile>` | Orchestration practices (if multi-instrument works are in the corpus) |
| `motivic_profile` | `MotivicStyleProfile` | Motivic development practices |
| `signature_patterns` | `Vec<SignaturePattern>` | Distinctive patterns specific to this composer |
| `sample_size` | `u32` | Number of works in the corpus |
| `confidence` | `f32` | Overall confidence in the profile (based on sample size and corpus quality) |

### 3.5 Harmonic Style Profile

**Definition 3.5.1**. Aggregate harmonic characteristics.

| Field | Type | Description |
|-------|------|-------------|
| `chord_vocabulary_size` | `u32` | Number of distinct chord types used |
| `chord_frequency` | `Map<String, f32>` | Normalised frequency of each Roman-numeral/chord label |
| `preferred_progressions` | `Vec<RankedProgression>` | Most frequent chord progressions |
| `modulation_frequency` | `f32` | Average modulations per work |
| `modulation_technique_preference` | `Map<ModulationTechnique, f32>` | Distribution of modulation techniques |
| `preferred_key_relationships` | `Map<String, f32>` | Distribution of named chromatic relationships in tonal-plan entries |
| `chromatic_density` | `f32` | Average proportion of chromatic chords per work |
| `secondary_dominant_frequency` | `f32` | How often secondary dominants appear |
| `augmented_sixth_frequency` | `f32` | How often augmented sixth chords appear |
| `neapolitan_frequency` | `f32` | How often Neapolitan chords appear |
| `harmonic_rhythm_mean` | `f32` | Average harmonic changes per bar |
| `harmonic_rhythm_variance` | `f32` | Variability of harmonic rhythm |
| `cadence_type_distribution` | `Map<CadenceType, f32>` | Preference for cadence types |
| `deceptive_cadence_frequency` | `f32` | How often expected cadences are evaded |
| `preferred_key_signatures` | `Map<String, u32>` | Preferred simplified key labels across tonal-plan entries |
| `tonal_ambiguity_index` | `f32` | Frequency of passages where key is ambiguous or contested |

**RankedProgression**:

| Field | Type | Description |
|-------|------|-------------|
| `progression` | `Vec<String>` | Roman numeral sequence |
| `frequency` | `f32` | Normalised frequency across the corpus |
| `rank` | `u32` | Rank by frequency |
| `contexts` | `Vec<String>` | Typical formal contexts where this progression appears |

### 3.6 Melodic Style Profile

**Definition 3.6.1**. Aggregate melodic characteristics.

| Field | Type | Description |
|-------|------|-------------|
| `interval_distribution` | `Map<Interval, f32>` | Normalised melodic interval distribution |
| `preferred_intervals` | `Vec<Interval>` | Top intervals by frequency |
| `conjunct_proportion` | `f32` | Overall proportion of stepwise motion |
| `average_phrase_length` | `f32` | In beats |
| `phrase_length_variance` | `f32` | Variability of phrase lengths |
| `contour_preferences` | `Map<ContourShape, f32>` | Normalised contour shape distribution |
| `typical_range` | `Interval` | Average melodic range |
| `chromaticism_rate` | `f32` | Average proportion of chromatic pitches in melodies |
| `scale_degree_emphasis` | `Map<u8, f32>` | Which scale degrees are emphasised |
| `ornament_density` | `f32` | Frequency of ornamental figures |
| `sequence_frequency` | `f32` | How often melodic sequences are used |
| `leitmotif_usage` | `bool` | Whether the composer uses recurring associative motifs |

### 3.7 Rhythmic Style Profile

**Definition 3.7.1**. Aggregate rhythmic characteristics.

| Field | Type | Description |
|-------|------|-------------|
| `duration_distribution` | `Map<String, f32>` | Normalised note-duration-label distribution |
| `preferred_durations` | `Vec<String>` | Note-duration labels ranked by frequency |
| `syncopation_index` | `f32` | Average syncopation across works |
| `rhythmic_variety` | `f32` | Entropy of the duration distribution |
| `preferred_metres` | `Map<String, u32>` | Distribution of canonical time-signature/grouping labels |
| `metrical_complexity` | `f32` | Frequency of irregular metres and changes |
| `tempo_mean`, `tempo_stddev` | `f32, f32` | Population mean and standard deviation of observed tempo samples |
| `rubato_tendency` | `f32` | Average rubato degree |
| `rhythmic_motif_consistency` | `f32` | How consistently rhythmic motifs recur within a work |

### 3.8 Formal Style Profile

**Definition 3.8.1**. Aggregate formal characteristics.

| Field | Type | Description |
|-------|------|-------------|
| `preferred_forms` | `Map<FormClassification, u32>` | Distribution of form types |
| `average_work_length` | `f32` | In bars |
| `section_proportions` | `Map<String, f32>` | Average proportional size of formal sections (across works of the same form) |
| `exposition_recapitulation_ratio` | `Option<f32>` | For sonata-form works: relative size of recapitulation to exposition |
| `development_proportion` | `Option<f32>` | For sonata-form works: proportion occupied by development |
| `introduction_frequency` | `f32` | How often works begin with a slow introduction |
| `coda_frequency` | `f32` | How often works end with a coda |
| `climax_placement` | `f32` | Average relative position of the work's climax (0.0–1.0) |
| `golden_ratio_adherence` | `f32` | How closely climax positions align with the golden ratio |
| `transition_technique` | `Vec<String>` | Common techniques for transitions between sections |

### 3.9 Voice-Leading Style Profile

**Definition 3.9.1**. Aggregate voice-leading characteristics.

| Field | Type | Description |
|-------|------|-------------|
| `parallel_fifths_tolerance` | `f32` | Frequency per work (0 = strictly avoided; higher = tolerated or intentional) |
| `parallel_octaves_tolerance` | `f32` | Same |
| `preferred_motion_type` | `String` | Most frequent motion type between outer voices |
| `voice_independence_index` | `f32` | Average melodic independence across voices |
| `common_tone_retention` | `f32` | Average common-tone retention rate |
| `leading_tone_resolution_rate` | `f32` | How consistently the leading tone resolves upward |
| `seventh_resolution_rate` | `f32` | How consistently chordal sevenths resolve downward |
| `spacing_preference` | `String` | Typical spacing (close, open, mixed) |
| `voice_crossing_tolerance` | `f32` | Frequency of voice crossings |

### 3.10 Textural Style Profile

| Field | Type | Description |
|-------|------|-------------|
| `average_density` | `f32` | Mean number of simultaneous voices |
| `density_range_low`, `density_range_high` | `f32, f32` | Minimum and maximum observed density samples |
| `texture_type_distribution` | `Map<String, f32>` | Preference for monophonic/homophonic/polyphonic/etc. |
| `register_span_preference` | `f32` | Average vertical span in semitones |
| `density_dynamic_correlation` | `f32` | Correlation between textural density and dynamic level |

### 3.11 Dynamic Style Profile

| Field | Type | Description |
|-------|------|-------------|
| `dynamic_range_low`, `dynamic_range_high` | `String, String` | Softest and loudest non-empty dynamic labels observed |
| `most_frequent_dynamic` | `DynamicLevel` | Most commonly used level |
| `dynamic_change_rate` | `f32` | Average changes per bar |
| `subito_frequency` | `f32` | Frequency of sudden dynamic changes |
| `climax_dynamic` | `DynamicLevel` | Typical dynamic at climactic moments |
| `dynamic_arc_shape` | `ContourShape` | Typical dynamic shape across a work (arch, ascending, etc.) |

### 3.12 Orchestration Style Profile

**Definition 3.12.1**. Present only if the corpus includes orchestral or multi-instrument works.

| Field | Type | Description |
|-------|------|-------------|
| `preferred_instruments` | `Map<String, f32>` | Instrument usage frequency |
| `signature_combinations` | `Vec<InstrumentCombination>` | Distinctive instrument combinations |
| `doubling_preferences` | `Vec<DoublingPattern>` | Preferred doublings |
| `melody_assignment_preference` | `Map<String, f32>` | Which instruments carry melody most often |
| `tutti_proportion` | `f32` | Proportion of the work in full orchestral tutti |
| `solo_proportion` | `f32` | Proportion featuring a solo instrument |
| `build_up_technique` | `Vec<String>` | Characteristic approaches to orchestral crescendo |
| `colour_signature` | `Vec<String>` | Distinctive timbral combinations associated with this composer |

### 3.13 Motivic Style Profile

| Field | Type | Description |
|-------|------|-------------|
| `thematic_economy` | `f32` | Average ratio: total material / distinct themes |
| `preferred_transformations` | `Map<ThematicTransformation, f32>` | Most frequently used transformations |
| `development_density` | `f32` | Average proportion of a work devoted to development of material |
| `fragmentation_frequency` | `f32` | How often themes are fragmented |
| `sequence_frequency` | `f32` | How often sequential treatment is used |
| `cross_movement_thematic_links` | `bool` | Whether the composer links movements thematically |

### 3.14 Signature Patterns

**Definition 3.14.1**. A detected *SignaturePattern* is an observed harmonic bigram whose
proportion is greater in this composer's analysed works than in analysed works owned by other
composers. Unassigned works and the target composer are excluded from the baseline. Each work
is counted once. Only length-two progression inventories are currently admitted; longer sparse
patterns do not supply a complete opportunity denominator.

For counts `x/n` and `y/m`, let `p=(x+y)/(n+m)`. The descriptive contrast is
`z=(x/n-y/m)/sqrt(p*(1-p)*(1/n+1/m))`. Detection retains positive contrasts at least 1.5 when
both populations have observations and the variance is positive. Identical rates produce no
signature. Missing target or comparison observations produce explicit unavailability. The
selection threshold is a heuristic: overlapping harmonic windows are not independent trials,
and this statistic supplies neither a calibrated significance level nor a prediction of style.
The MCP result exposes the observation denominators and contributing work identities; each
pattern retains its target occurrences and a description of both counts.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<SignaturePattern>` | Unique identifier |
| `description` | `String` | Human-readable description |
| `domain` | `PatternDomain` | Closed analytical domain enum |
| `pattern_data` | `PatternData` | The pattern itself |
| `distinctiveness` | `f32` | Descriptive pooled-proportion z for newly detected harmonic bigrams; historical supplied values retain their original provenance |
| `examples` | `Vec<(Id<IngestedWork>, ScoreTime)>` | Specific instances |

**PatternData** is the current four-alternative C++ sum type. `PatternDomain` supplies the musical
meaning; the value alternative supplies the closed persisted shape:

| Variant | Description | Example |
|---------|-------------|---------|
| `Vec<String>` | Symbolic sequence | Harmonic progression or cadential approach |
| `Vec<i8>` | Signed interval cell | Melodic cell |
| `Vec<f32>` | Numeric sequence | Rhythmic figure or formal proportions |
| `String` | Described gesture/signature | Textural, registral, dynamic, or orchestrational description |

The v3–v4 JSON tags are respectively `string_sequence`, `interval_sequence`, `float_sequence`, and
`text`. No domain-dependent guessing occurs during decoding.

### 3.15 Deterministic aggregation and tractability boundary

For a composer or period, let W be its unique member works whose `analysis_complete` flag is true.
A rebuild starts from a neutral `StyleProfile`, sets `sample_size` to `n`, the number of works in
W, and sets `confidence = min(0.95, 1 - 1/(0.2n + 1))` when W is non-empty. It then applies the closed
rules below. A normalised count distribution divides each merged count by the merged total. A
per-work mean divides by the number of eligible observations for that individual field. The
runtime dependency catalogue maps each profile field to the analysis fields its producer consumes.
An unavailable source domain, an unavailable source field (including descendants of a named
unavailable parent), or an undefined required sample contributes neither a value nor a denominator.
Cross-domain fields use their actual sources: formal climax placement consumes dynamic evidence,
and density/dynamic correlation consumes both curves. Legacy supplied values remain unqualified
and retain their supplied meaning. An eligible explicit zero contributes as zero. Empty melodic
sentinels, empty tempo curves, unmatched or constant correlation curves, and absent orchestration
cannot denote their respective observations. Conditional melodic limits apply per lane: one-note
lanes remain eligible for range and chromaticism, but do not contribute to conjunctness when the
analysis marks fewer-than-two-attack conjunctness unavailable. A partial-bar-proportion limitation
currently excludes that work's proportional fields conservatively, since the stored formal record
has no offsets identifying only its partial sections.

| Domain | Deterministic aggregate from `WorkAnalysis` |
|--------|------------------------------------------------|
| Harmonic | Merge chord counts; normalise them and progression occurrence counts; rank progressions by descending occurrence then lexical sequence; union and lexically order their contexts. Average modulation count, harmonic-rhythm mean/variance, chromatic-event count divided by each work's chord count, and named chromatic/cadence event counts per work. Normalise modulation techniques, tonal-plan relationship entries, and cadence codes. Count tonal-plan key labels. `deceptive_cadence_frequency` is DC events per work. |
| Melodic | Exclude an empty voice sentinel using `note_count` plus the legacy explicit-evidence fields. Merge and normalise interval, contour, and scale-degree counts; rank intervals by descending count then signed interval. Average voice conjunctness, chromaticism, and `range_high-range_low`. `sequence_frequency` is sequential thematic occurrences divided by all thematic occurrences. |
| Rhythmic | Merge and normalise duration counts; rank duration labels by descending count then lexical label; merge metre counts; compute base-2 Shannon entropy over duration frequencies. Average syncopation, metrical complexity, and rubato per work. Tempo is the population mean and population standard deviation of every tempo-profile sample. Motif consistency is motifs with more than one occurrence divided by all catalogued motifs. |
| Formal | Count form codes and average total bars. For each section label, average the supplied proportional observations. Average recapitulation/exposition ratios only for works containing a positive exposition and a named recapitulation, and development only where a development observation is named; supplied zero recapitulation/development remains a measured zero. A slow introduction requires the earliest section to be labelled introduction and have a lower positive tempo than the next section; a coda must be the final section. Transition `character` values are ranked by descending work-record count then lexical value. Climax position is an evidence-only mean. Golden adherence per observed climax is `clamp(1 - abs(position - 0.618)/0.618, 0, 1)`. |
| Voice leading | Average parallel-fifth, parallel-octave, and crossing counts per work; average common-tone retention and independence. The preferred motion is the greatest aggregate proportion in deterministic order contrary, oblique, similar, parallel and stays empty if all are zero. Leading-tone and seventh resolution rates are weighted by each `ResolutionPattern.frequency`. Spacing counts at or below 12 semitones are close; above 12 are open; at least 60% selects close/open, otherwise mixed. |
| Textural | Average work density and register span; take the global minimum/maximum density sample; sum each texture proportion and divide by the number of works eligible for that field. For each work, inner-join density and dynamic curves by exact `ScoreTime`, compute Pearson correlation only with at least two samples and non-zero variance, then average the defined correlations. |
| Dynamic | Select the softest/loudest non-empty labelled extremes, merge dynamic-level counts, and average change/subito counts per work. For each non-empty shape, map its maximum intensity to the nearest declared ordinary dynamic. A range at or below `1e-5` is stationary; an interior maximum/minimum at least 25% of the range beyond both endpoints is arch/inverted arch; at least two non-zero direction changes is oscillating; otherwise endpoint displacement of at least ±25% is ascending/descending, with the residual complex. Modal values are retained with stable enum/key tie-breaking. No shape leaves the neutral `Stationary` default. |
| Orchestration | Include only present orchestration records. Average instrument-use and melody-carrier maps over those records. Canonicalise a combination's instrument set lexically, merge exact combination and doubling keys by summed frequency, and order them by key. Build-up techniques contain `register expansion` when declared and `instrument accretion` when a crescendo has more than one ordered entry. |
| Motivic | Average thematic economy and density per work. Merge and normalise transformation-event codes. Fragmentation and sequence frequencies divide their corresponding event counts by transformation events from works eligible for the respective classification. |

The following style fields currently have no source evidence carrier and therefore remain neutral
after every deterministic rebuild: `tonal_ambiguity_index`, `average_phrase_length`,
`phrase_length_variance`, `ornament_density`, `leitmotif_usage`, orchestral `tutti_proportion`,
`solo_proportion`, and `colour_signature`, and `cross_movement_thematic_links`. Zero/false/empty in
these positions means **unavailable in the current model**, not an observed absence. Signature
patterns are also excluded from deterministic rebuild because their distinctiveness requires the
separate cross-corpus detection lifecycle.

The WorkAnalysis carrier is broader than the automatic Score analyser. For example, supplied
resolution patterns, identified thematic transformations, and orchestral crescendo patterns are
tractable once present, but the current automatic analyser does not infer every such record. A
field is therefore described as (1) automatically produced, (2) derivable from supplied carrier
evidence, or (3) unavailable; persistence completeness must never be used to upgrade (2) or (3)
into an automatic-analysis claim.

The current `analyze_score` boundary is exact:

| Domain | Automatically produced from Score | Explicitly unavailable in automatic analysis |
|---|---|---|
| Harmonic | Exact sounding boundaries and active key/mode; supplied annotations or finite chord/Roman-numeral/cadence recognition; chord vocabulary, bigrams, cadence codes, harmonic rhythm, tonal plan | Modulation inventory, named chromatic techniques, tonicisation frequency, section rates, cadence approach and structural status |
| Melodic | Every `(PartId, voice_index)` lane selects its highest newly attacked MIDI note at each exact onset after individual tie folding; directed intervals include the full ±127 domain; range, 10th/90th note-order-statistic tessitura, active-key degrees/chromaticism, primary lane by selected attack count | Contours, leap resolution, ascending/descending runs; range/tessitura/chromaticism for empty lanes and conjunctness with fewer than two attacks |
| Rhythmic | Individual folded note durations and attacks retain chord/unison multiplicity; explicit-rest proportion counts voice allocations once; grouped-meter sustain syncopation; global/local metre inventory; instantaneous effective quarter-BPM at tempo events; section attacks per touched global bar | Rhythmic motifs, rubato |
| Formal | Core finite label-pattern classifier, including ABACA rondo; supplied sections with bar-index lengths/proportions, exact section-start key/mode and effective quarter-BPM including ramps/metric modulation, declared tonal plan | Thematic assignment, symmetry, section character/subsections; form classification without sections, lengths/proportions for partial-bar sections |
| Voice leading | Exact sounding-boundary slices select the highest sounding MIDI note per lane; all simultaneous lane pairs; moving consecutive-pair motion, equal nonzero displacement for parallel perfect intervals, strict order reversal for crossings, compound spacing, previous-slice unique-pitch retention | Independence and tendency-tone resolutions; all motion-dependent fields when no consecutive moving pair exists |
| Textural | Global-bar Part presence and note-register inventories, their means and section density | Mono-/homo-/polyphonic classification and spacing profile |
| Dynamic | Explicit written note marks and hairpins; typed intensity ordering; traversal-order changes per bar, latest visited mark carried by bar, first maximum as climax | Section dynamic ranges; mark-dependent fields when only hairpins are supplied |
| Orchestration | Multi-Part instrument-name bar presence (union across same-name Parts) and highest visited MIDI pitch as a bar melody-carrier heuristic | Combinations, doublings, orchestral crescendos, density/orchestration correlation; the whole domain for single-Part scores |
| Motivic | Exhaustive contiguous 3–8 selected-note windows per lane, matched by exact directed intervals and rational duration ratios; greedy nonoverlap within a lane and independent occurrences across lanes; repetition, exact transposition and uniform duration scaling; exact Part/voice/start/end/key context and union-of-covered-attacks density | Thematic economy, developmental techniques, fragmentation and sequential-repetition classifications/frequencies (including the copied melodic sequence frequency); the whole domain when no eligible window exists |

All nine automatic evidence entries are `Heuristic`: exact symbolic arithmetic does not establish
that the musical selection or interpretation is exhaustive. Zero candidate matches with eligible
windows is a measured finite absence; zero eligible windows is unavailable. Matching uses rational
durations, while persisted float rhythmic cells retain original whole-note durations as summaries.

Syncopation is the share of nongrace attacks whose folded allocation sustains strictly beyond a
later stronger boundary. Strength is 3 at the bar downbeat, 2 at a later grouped beat start, 1 at an
interior denominator-unit pulse, and 0 elsewhere. Ending exactly at a boundary does not count;
ties across bars do, and local grouping is evaluated within the global bar frame. Form classification
uses supplied labels. Texture counts bar inventories and cannot establish sounding simultaneity.
Voice leading examines all simultaneous structural lanes at exact event boundaries, using their
highest sounding pitch. These are declared finite evidence functions, not musicological equivalence
claims. Uncomputed fields remain explicitly unavailable even when another field in the same domain
has observations. All-rest or grace-only works have no eligible syncopation denominator; a
nongrace downbeat attack supplies a valid measured zero. Voice-leading spacing, moving pairs,
and previous unique-pitch retention have independent observations: stationary reattacked notes
can supply retention without pitch motion, and one lane can supply retention without pair spacing.

---

## 4. Comparative Analysis

### 4.1 Cross-Composer Comparison

The Corpus IR supports comparison between any two ComposerProfiles across all analytical domains. This serves two purposes: it helps the agent understand what makes a composer *distinctive* (by contrast with others), and it enables style blending or interpolation (composing "in the harmonic language of Debussy with the formal proportions of Brahms").

**Definition 4.1.1**. A *StyleComparison* is the result of comparing two profiles.

| Field | Type | Description |
|-------|------|-------------|
| `composer_a` | `Id<ComposerProfile>` | First composer |
| `composer_b` | `Id<ComposerProfile>` | Second composer |
| `harmonic_divergence` | `f32` | Statistical distance between harmonic profiles |
| `melodic_divergence` | `f32` | Statistical distance between melodic profiles |
| `rhythmic_divergence` | `f32` | Statistical distance between rhythmic profiles |
| `formal_divergence` | `f32` | Statistical distance between formal profiles |
| `most_similar_dimensions` | `Vec<String>` | Domains where the composers are most alike |
| `most_divergent_dimensions` | `Vec<String>` | Domains where they differ most |
| `shared_patterns` | `Vec<SignaturePattern>` | Patterns common to both |
| `distinctive_to_a` | `Vec<SignaturePattern>` | Patterns present in A but not B |
| `distinctive_to_b` | `Vec<SignaturePattern>` | Patterns present in B but not A |

Statistical distance is computed using the Jensen-Shannon divergence for probability distributions and Euclidean distance for scalar values, normalised to a [0, 1] scale.

### 4.2 Evolutionary Analysis

For a single composer with works spanning a long period, the Corpus IR can analyse stylistic evolution by comparing PeriodProfiles.

**Definition 4.2.1**. An *EvolutionaryAnalysis* tracks how a composer's style changes over time.

| Field | Type | Description |
|-------|------|-------------|
| `composer` | `Id<ComposerProfile>` | The composer |
| `periods` | `Vec<PeriodProfile>` | Chronological period profiles |
| `trends` | `Vec<EvolutionaryTrend>` | Identified trends across periods |

**EvolutionaryTrend**:

| Field | Type | Description |
|-------|------|-------------|
| `dimension` | `String` | Which aspect is changing (e.g., "harmonic complexity", "chromatic density") |
| `direction` | `TrendDirection` | `Increasing`, `Decreasing`, `NonMonotonic`, `Stable` |
| `early_value` | `f32` | Characteristic value in the earliest period |
| `late_value` | `f32` | Characteristic value in the latest period |
| `description` | `String` | e.g., "Harmonic vocabulary expands from 12 chord types to 28" |

This is how the system captures observations such as Beethoven's increasing formal freedom, Debussy's growing harmonic ambiguity, or Stravinsky's shift from late-Romantic to neoclassical to serial practice.

---

## 5. Query Interface

### 5.1 Composition-Time Queries

These queries are designed for use during active composition, when the agent needs to make an informed creative decision.

| Query | Parameters | Returns |
|-------|-----------|---------|
| `GetStyleProfile` | composer_id | Complete StyleProfile |
| `GetHarmonicVocabulary` | composer_id | Ranked list of chord types with frequencies |
| `GetPreferredProgressions` | composer_id, context (optional section type) | Top progressions for a given formal context |
| `GetMelodicTendencies` | composer_id | Interval distribution, contour preferences, phrase lengths |
| `GetCadencePreferences` | composer_id | Cadence type distribution and approach patterns |
| `GetModulationTechniques` | composer_id | Preferred modulation techniques and key relationships |
| `GetFormalProportions` | composer_id, form_type | Typical section proportions for a given form |
| `GetOrchestrationPatterns` | composer_id, context | Instrument combinations for a given musical context |
| `GetSignaturePatterns` | composer_id | Distinctive patterns with examples |
| `FindExamples` | composer_id, criterion | Specific passages matching a criterion |
| `CompareComposers` | composer_a, composer_b | StyleComparison across all domains |
| `GetEvolution` | composer_id | EvolutionaryAnalysis |
| `HowWouldXHandle` | composer_id, situation_description | Best-matching examples and statistical tendencies for a described compositional situation |

### 5.2 The HowWouldXHandle Query

**Definition 5.2.1**. The currently supported *HowWouldXHandle* query retrieves annotated
sections by lexical criteria and aggregates observations from those passages. It does not
predict a composer's decisions from arbitrary natural-language situations.

| Field | Type | Description |
|-------|------|-------------|
| `composer_id` | `Id<ComposerProfile>` | Which composer |
| `situation` | `String` | Natural-language description of the compositional problem |
| `focus` | `Vec<String>` | Which analytical domains to prioritise (optional) |

**Example situations**:

- "Transitioning from the exposition to the development in a sonata-form movement in C minor"
- "Writing a lyrical second theme contrasting with an energetic first theme"
- "Building an orchestral climax from pianissimo strings to fortissimo tutti"
- "Handling the retransition back to the tonic before the recapitulation"
- "Writing a coda that references the opening material"

Every significant case-insensitive alphanumeric query token must occur in the section label or
character annotation. Articles, the prepositions `in`, `of`, `at`, and the words `section`,
`sections`, `passage` are ignored. Substring fragments do not match words. An empty significant
query or an absent annotated context returns no contextual evidence, rather than substituting
global style averages. The complex example situations above require matching annotations; they
are not claims of semantic inference.

The current tendencies are recognised harmonic changes and symbolic attack counts per bar.
Only passages with complete per-bar observations contribute; overlapping passage annotations
count each `(work, bar)` once. Missing observations are unavailable, whereas measured zero
counts remain valid. Signatures are returned only when their supporting occurrences fall inside
a matching passage. Global style information remains separately available from
`query_style_profile`. The query returns:

| Field | Type | Description |
|-------|------|-------------|
| `relevant_examples` | `Vec<AnnotatedExample>` | Specific passages from the corpus |
| `statistical_tendencies` | `Vec<Tendency>` | Aggregate patterns relevant to the situation |
| `signature_patterns` | `Vec<SignaturePattern>` | Composer-specific patterns applicable here |

**AnnotatedExample**:

| Field | Type | Description |
|-------|------|-------------|
| `work_id` | `Id<IngestedWork>` | Which work |
| `region` | `(ScoreTime, ScoreTime)` | Which passage |
| `relevance_score` | `f32` | Lexical token coverage (currently 1 for a complete match), not calibrated semantic relevance |
| `analysis_summary` | `String` | Concise analytical description of what happens in this passage |
| `harmonic_reduction` | `Vec<String>` | Roman numeral analysis of the passage |
| `formal_context` | `String` | Where in the formal structure this passage occurs |

**Tendency**:

| Field | Type | Description |
|-------|------|-------------|
| `domain` | `String` | Analytical domain |
| `observation` | `String` | e.g., "In development sections, this composer increases harmonic rhythm by 40% relative to exposition" |
| `confidence` | `f32` | 1 for exact aggregation of supplied observations; not calibrated analytical or statistical confidence |
| `supporting_examples_count` | `u32` | Number of fully observed matching annotated passages |

MCP passage responses expose `region_start` and `region_end` in exact ScoreTime coordinates.
Context responses additionally report `available`, `statistics_available`, a declared `method`
and an actionable unavailable reason. These fields distinguish a matching annotation, measured
local statistics and unavailable evidence.

### 5.3 Corpus-Level Queries

| Query | Parameters | Returns |
|-------|-----------|---------|
| `ListComposers` | — | All ComposerProfiles with corpus sizes |
| `ListWorks` | composer_id (optional) | All IngestedWorks, optionally filtered by composer |
| `SearchByPattern` | pattern_data, corpus_scope | Works/passages containing the specified pattern |
| `GetCorpusStatistics` | — | Aggregate statistics across the entire corpus |
| `MostDistinctiveFeature` | composer_id, domain | Single most statistically distinctive feature |

---

## 6. Ingestion and Analysis Workflow

### 6.1 MCP Tools

**Ingestion tools**:

| Tool | Description |
|------|-------------|
| `ingest_midi` | Ingest a MIDI file into the corpus |
| `ingest_musicxml` | Ingest a MusicXML file |
| `ingest_batch` | Ingest an explicit array of MIDI/MusicXML work payloads |
| `create_composer_profile` | Create a new ComposerProfile |
| `create_ingested_work` | Create a metadata-only work entry |
| `remove_ingested_work` | Remove a work plus all composer/period reverse references without reusing its session id |
| `assign_work_to_composer` | Associate an ingested work with a composer |
| `assign_work_to_period` | Assign a work to a compositional period within a composer's profile |
| `add_period_profile` | Add a named compositional period to a composer |
| `set_work_metadata` | Transactionally update metadata; a non-empty period resolves through actual membership and an empty period clears membership |

**Analysis tools**:

| Tool | Description |
|------|-------------|
| `analyze_work` | Run full analytical decomposition on an ingested work |
| `rebuild_style_profile` | Recompute a composer's style profile from all their works |
| `compare_composers` | Generate a StyleComparison between two profiles |
| `analyze_evolution` | Generate an EvolutionaryAnalysis for a composer |
| `detect_signature_patterns` | Identify statistically distinctive patterns for a composer |

**Composition-time query tools**:

| Tool | Description |
|------|-------------|
| `query_style_profile` | Get any aspect of a composer's style profile |
| `query_how_would_x_handle` | Get insights for a described compositional situation |
| `find_examples` | Find passages matching specific analytical criteria |
| `get_progression_examples` | Find examples of a specific chord progression in a composer's corpus |
| `get_formal_template` | Get typical formal proportions and tonal plan for a form type from a composer |

**Corpus inspection tools**:

| Tool | Description |
|------|-------------|
| `validate_corpus` | Validate the corpus and return diagnostics, with `valid` (no Error diagnostic) and `loadable` (no load-blocking structural Error) |
| `get_corpus_json` | Serialise the complete in-memory corpus |

### 6.2 Complete Ingestion Workflow

1. **Collect**: Gather MIDI or MusicXML files for a composer.
2. **Create profile**: `create_composer_profile("Sergei Rachmaninov", birth_year=1873, death_year=1943)`.
3. **Ingest**: call `ingest_batch` with an array of `{title, midi_base64/musicxml, composer_id, format}` objects. Each supplied document is processed through the ingestion pipeline (§1).
4. **Assign**: `assign_work_to_composer(work_id, composer_id)` for each ingested work. Set metadata: title, opus, year, instrumentation, whether it is a reduction.
5. **Assign periods** (optional): `assign_work_to_period(work_id, composer_id, "Early")`, etc.
6. **Analyse**: `analyze_work(work_id)` for each work. This runs the full analytical decomposition (§2) using the theory engine and synchronously refreshes affected composer and period aggregates.
7. **Verify/rebuild profile** (optional): `rebuild_style_profile(composer_id)` deterministically recomputes the composer and period StyleProfiles from their analysed work memberships. Normal lifecycle workflows already perform this refresh; the explicit operation is useful after assembling or migrating a value outside those workflows.
8. **Detect signatures**: `detect_signature_patterns(composer_id)`. This reports descriptive harmonic-bigram contrasts against other composers, with explicit observation denominators and unavailable comparison evidence.
9. **Query**: The profile is now available for composition-time queries.

### 6.3 Composition Workflow Integration

During composition, the agent interleaves production operations (Score IR, Timbre IR, Mix IR) with Corpus IR queries:

1. Agent is composing a piano concerto in the style informed by Rachmaninov.
2. Agent queries: `get_formal_template(composer_id, form_type)` → receives typical formal proportions, tonal plan, section characteristics.
3. Agent creates the Score IR with `score_create`, then applies a formal plan based on the template.
4. Agent queries: `query_how_would_x_handle(composer_id, "Opening of the first movement, establishing the main theme in solo piano before orchestral entry")` → receives examples from Rachmaninov's concerto openings, statistical tendencies for phrase lengths, typical key and dynamic profile.
5. Agent writes the opening melody with `score_write_melody` into the piano part, drawing on the melodic style profile (interval distribution, contour preferences, chromaticism rate).
6. Agent queries: `get_progression_examples(composer_id, ["i", "iv6", "V7", "i"])` → confirms this is within the harmonic vocabulary and finds examples of how Rachmaninov typically continues after this progression.
7. Agent continues composing, querying the Corpus IR whenever a creative decision would benefit from historical grounding.

The Corpus IR does not make the decisions. It informs them. The agent remains the creative agent; the corpus provides the education.

---

## 7. Storage

### 7.1 Corpus Database

The current Corpus IR is an in-memory `CorpusDatabase` value that serialises as one versioned JSON document. Persistence is caller-owned.

**Storage components**:

| Component | Format | Description |
|-----------|--------|-------------|
| Ingested Score IRs | Embedded JSON | Score IR for each ingested work |
| Work Analyses | Embedded JSON | Analytical decomposition per work |
| Composer Profiles | Embedded JSON | Style profiles with aggregated statistics |
| Signature Patterns | Embedded JSON | Per-composer distinctive patterns |

There is no runtime SQLite pattern index, comparison cache, or binary storage backend.

#### 7.1.1 JSON schema and migration boundary

Corpus JSON schema version 5 is the current write format. Its strict projection covers every
authoritative field of `IngestedWork`, `WorkMetadata`, `IngestionConfidence`, `WorkAnalysis`,
`ComposerProfile`, `PeriodProfile`, `StyleProfile`, and `SignaturePattern`, including optional
orchestration, composer active periods, all tonal plans and thematic structures, and the complete
`PatternData` value. Optional values are omitted when disengaged; all non-optional fields are
required. Numeric-key maps use ordered arrays of exact `{key, value}` records so signed and narrow
integer keys are not reinterpreted as JSON object names. `PatternData` is an explicit tagged record
whose `kind` is one of `string_sequence`, `interval_sequence`, `float_sequence`, or `text`.

The version-1 reader remains a deliberately lenient migration path: it accepts flat ScoreTime
records and fills historical omissions with defaults, and full-corpus v1 loads do not run
validate-on-load. Version 2 remains a strict reader for the historical partial projection; fields
that v2 never represented migrate to their declared C++ defaults rather than being fabricated from
other statistics. Version 3 uses nested ScoreTime, rejects missing required fields, range-checks
integer and enum encodings, rejects unknown `PatternData` tags, and was field-complete for its
historical model. Version 4 adds the required `duration_quantisation_residual`, requires every
confidence dimension to be finite and in `[0,1]`, and requires both residuals to be finite and
non-negative. Versions 1–3 migrate the historically unobserved duration residual to zero; this is
a migration default, not evidence that old ingestion preserved durations. Full-corpus loads of
versions 2–5 run validation, but only the structural Error rules (C2, C14, C15, C16, and C17) block
loading; ingestion and analysis quality findings (C1 and C3–C13) describe the evidence rather than
the document's integrity, so a state produced by Sunny's own tools always reloads, and they remain
diagnostics for `validate_corpus`. Version-2 migration loads retain their historical pre-freshness
validation contract and therefore skip C15. Version 5 adds required per-domain method/availability
evidence and nullable exact melodic voice identities and thematic end positions. Versions 1–4
retain supplied analysis values with absent evidence and voice/end provenance; they do not invent
voice zero or a passage end. A v5 standalone work also rejects contradictory C17 evidence.
Writers emit v5 only. A supported-schema document round-trips every value represented by that schema; only v5 is
field-complete with respect to the current Corpus IR.

### 7.2 Incremental Updates

When ownership, analysis, period membership, or work existence changes through a lifecycle workflow, affected composer and period aggregates are rebuilt synchronously before the operation returns. Composer signature patterns are invalidated when their underlying composer membership or analysis set changes and are populated only by the explicit detection workflow. Period-only changes preserve composer signatures. There is no background refresh or background persistence. Directly assembled C++ values and migration loads can retain supplied aggregates until `rebuild_style_profile` or another lifecycle workflow is invoked; C15 reports this stale state during direct validation, and a v3–v5 corpus load rejects it. Strict v2–v5 corpus loads reject graph contradictions through C14.

Each `WorkAnalysis.evidence` entry uses one of `Unqualified`, `ExactSymbolic`, `Heuristic`, or
`Unavailable`, with a named method, observation count and explicit unavailable field paths.
Computed entries require positive observations and no unavailable reason. Unavailable entries
require a reason and zero observations. Missing legacy entries are unqualified. Only an explicitly
unavailable source domain or field is excluded from that profile field's values and denominator;
supplied unqualified values remain represented and are reported as such by MCP inspection. The
style confidence is a sample-size heuristic, not a probability of analytical accuracy.
`get_work_analysis` exposes the complete record, each domain's qualification and its unavailable
field paths. `query_style_profile` retains domain counts and adds `availability[domain].fields[name]`
for every profile field: computed, unavailable, unqualified and contributing work counts, source
field dependencies and methods, availability/status and qualification. Counts refer to distinct
analysed works eligible for that field; they are not attack, lane or curve-sample weighting counts.
A domain-level computed status does not qualify every field. Fields without an aggregate producer
have unavailable field status even when other fields in their domain have observations.

---

## 8. Validation

### 8.1 Ingestion Validation

| Rule | Severity | Description |
|------|----------|-------------|
| C1 | Warning | Ingestion confidence below 0.7 in any dimension |
| C2 | Error | Ingested Score IR fails structural validation |
| C3 | Warning | Key estimation confidence below 0.5 (key may be incorrect) |
| C4 | Info | MIDI work has no time signature metadata (ingester defaulted to 4/4) |
| C5 | Warning | Voice separation produced more than 6 voices (possible error) |

### 8.2 Analysis Validation

| Rule | Severity | Description |
|------|----------|-------------|
| C6 | Error | Harmonic analysis coverage, the proportion of the work's bars that carry at least one recognised chord, is less than 80%; a work with no recognised chord has zero coverage |
| C7 | Warning | Formal segmentation produced sections shorter than 4 bars (possible over-segmentation) |
| C8 | Warning | No thematic units identified (work may be too short or too complex for automated extraction) |
| C9 | Info | Orchestration analysis not possible (single-instrument work) |

### 8.3 Profile Validation

| Rule | Severity | Description |
|------|----------|-------------|
| C10 | Warning | Style profile based on fewer than 5 works (low statistical reliability) |
| C11 | Info | Style profile based on fewer than 10 works (moderate reliability) |
| C12 | Warning | Period profile has fewer than 3 works (insufficient for period characterisation) |
| C13 | Info | Signature pattern distinctiveness below 1.5 standard deviations (may not be truly distinctive) |
| C14 | Error | Corpus map identities, composer ownership, period membership, or bounded period ranges contradict one another |
| C15 | Error | A composer or period's deterministic non-signature StyleProfile differs from the aggregate of its unique analysed work membership |
| C16 | Error | A confidence dimension is non-finite or outside `[0,1]`, or an onset/duration quantisation residual is non-finite or negative |
| C17 | Error | An analysis evidence domain/kind/method/count/reason contradicts its availability, unavailable fields repeat, or optional thematic span/voice or melodic lane provenance is invalid or contradicts its embedded Score. A thematic voice belongs to its starting measure; a melodic lane must occur in a measure of the named Part. Legacy absent voice indices remain unqualified. |

---

## 9. Cross-Specification Integration

### 9.1 The Five-Layer Stack

The complete Sunny system:

| Layer | Specification | Domain | Provides |
|-------|--------------|--------|----------|
| 1. Theory | Theory Spec | Musical vocabulary | Pitch, intervals, chords, scales, voice leading, form, transformations |
| 2. Score | Score IR Spec | Musical content | Notes, rhythms, dynamics, articulations, formal structure |
| 3. Timbre | Timbre IR Spec | Sound identity | Synthesis, sampling, per-instrument effects |
| 4. Mix | Mix IR Spec | Production | Levels, EQ, compression, spatial positioning, mastering |
| 5. Corpus | Corpus IR Spec | Musical knowledge | Style profiles, compositional patterns, historical examples, creative insights |

Layers 1–4 provide executive capacity: the ability to produce music. Layer 5 provides deliberative capacity: the knowledge to produce music *worth producing*.

**Dependency structure**:

```
Theory Spec ←── Score IR ←── Timbre IR
     ↑              ↑            ↑
     │              │            │
     │         Mix IR ───────────┘
     │              ↑
     │              │
     └── Corpus IR ─┘
```

The Corpus IR depends on the Theory Spec (it uses the theory engine for analysis) and on the Score IR (ingested works are stored as Score IR documents). It does not depend on the Timbre IR or Mix IR (corpus analysis is concerned with compositional content, not production parameters). The Mix IR's reference profile system (§8 of the Mix IR Spec) is a parallel but independent concept: it analyses *recordings* for production characteristics, while the Corpus IR analyses *scores* for compositional characteristics.

### 9.2 Tool Count

The full MCP tool set across the IR specifications and aggregate project model:

| Registration group | Tools | Examples |
|--------------------|------:|----------|
| Core and Ableton | 11 | `analyze_harmony`, `voice_lead`, `get_ableton_session_state`, `get_ableton_remote_log` |
| Score IR | 52 | `score_create`, `score_remove_part`, `score_reorder_parts`, `score_compile_to_musicxml` |
| Timbre IR | 28 | `set_sound_source`, `map_timbre_parameter`, `validate_timbre` |
| Mix IR | 33 | `set_channel_relative_level`, `resolve_mix_fader_levels`, `validate_mix` |
| Corpus IR | 23 | `ingest_midi`, `get_work_analysis`, `query_how_would_x_handle` |
| Project and workspace | 11 | `create_project`, `bind_project`, `get_project_json`, `project_plan_to_ableton`, `project_apply_ableton_plan` |

Total: 158 MCP tools. `tools/list` is the runtime authority.

---

## 10. Runtime Invariants and Validation Boundaries

### 10.1 Structural

1. An embedded Score, when present, is checked by Score structural validation.
2. `assign_work_to_composer` gives a work at most one ComposerProfile owner; reassignment removes stale composer and period references.
3. Empty and small ComposerProfiles are representable; C10–C12 report insufficient sample sizes.
4. Successful lifecycle mutations synchronously recompute affected composer and period StyleProfiles from unique analysed work memberships; `rebuild_style_profile` provides an explicit deterministic refresh.

### 10.2 Analytical

5. C1–C9 and C16 expose low or malformed confidence, structural errors, weak harmonic coverage, missing thematic material, and other analytical limitations as diagnostics; ingestion does not manufacture coverage or a section plan to satisfy a threshold. C16 is evaluated before C1/C3 so a NaN cannot evade threshold comparisons.
6. StyleProfile records `sample_size` and a bounded confidence derived from it.

### 10.3 Consistency

7. `assign_work_to_period` requires a coherent matching composer ownership and gives a work at most one PeriodProfile across the corpus graph; clearing updates both metadata and membership.
8. Signature patterns are populated only by the explicit detection workflow and are invalidated when their underlying composer analysis set changes.
9. `remove_ingested_work` removes all reverse references, refreshes affected aggregates, and never rewinds the session id sequence. Failed lifecycle and mixed metadata/period operations leave corpus state unchanged.
10. C14 checks root map key/embedded-id agreement, ownership and reverse references, signature-example membership, unique period membership, metadata agreement, unique labels, valid ranges, and non-overlap for fully bounded period ranges.
11. C15 compares every composer and period's deterministic aggregate against its analysed membership after C14 succeeds. Explicit signature patterns are excluded because they belong to the separate detection lifecycle. Version-2 migration loads skip this newer invariant; direct validation and v3/v4 loads enforce it.
12. `IngestionConfidence` dimensions are finite values in `[0,1]`; onset and duration quantisation residuals are finite non-negative whole-note RMS errors. A zero migrated duration residual in schema v1–v3 means “historically absent,” not “measured lossless.”

---

*End of specification.*
