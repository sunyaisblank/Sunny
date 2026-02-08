# Sunny Corpus IR — Formal Specification

**Version:** 0.1.0-draft  
**Date:** 2026-02-08  
**Status:** Initial specification; subject to iterative refinement  
**Dependencies:** Sunny Engine Formal Specification v0.1.0 (the "Theory Spec"); Sunny Score IR Specification v0.1.0 (the "Score IR Spec")

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
Existing Music (MIDI, MusicXML, Audio)
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

- Statistical distributions are represented as histograms, kernel density estimates, or parametric distribution fits, depending on the property.
- Frequencies of occurrence are given as counts or proportions (0.0–1.0) relative to the analytical unit (per piece, per phrase, per harmonic change).
- Confidence intervals and sample sizes are reported alongside statistical summaries to prevent overconfident generalisation from small corpora.

---

## 1. Ingestion Pipeline

### 1.1 Overview

The ingestion pipeline converts external music representations into Score IR documents suitable for analysis. This is a one-way, lossy transformation: the external format is consumed and converted; the resulting Score IR is an analytical representation, not a publication-quality score. The goal is to capture the compositional substance — harmonic content, melodic material, rhythmic structure, formal proportions, dynamic shape — with sufficient fidelity for meaningful statistical and structural analysis.

### 1.2 Supported Input Formats

| Format | Fidelity | Primary Use Case |
|--------|----------|-----------------|
| MIDI (SMF Type 0/1) | Medium | Piano reductions, keyboard works, sequenced arrangements |
| MusicXML | High | Full scores with enharmonic spelling, articulations, dynamics, lyrics |
| MEI (Music Encoding Initiative) | High | Scholarly editions, critical apparatus |
| ABC Notation | Medium | Folk music, lead sheets |
| Humdrum **kern | High | Analytical corpora (e.g., kern.humdrum.org) |
| MusicXML (compressed .mxl) | High | Same as MusicXML, compressed |

### 1.3 MIDI Ingestion

MIDI is the most abundant format for existing music in digital form. Piano reductions of orchestral works, keyboard compositions, and sequenced arrangements are widely available as MIDI files. MIDI ingestion is therefore the most critical pipeline.

**Challenges and solutions**:

| Challenge | Description | Solution |
|-----------|-------------|----------|
| No enharmonic spelling | MIDI stores note numbers (0–127), not spelled pitches | Spelling inference from key context and melodic interval patterns [H] |
| No explicit key signatures | Key must be inferred from pitch content | Key estimation via the Krumhansl-Schmuckler algorithm or profile correlation [H] |
| No barline positions | Only tick offsets from the start of the file | Metre inference from onset patterns and quantisation grid [H] |
| No formal structure | No section markers, phrase boundaries, or labels | Formal segmentation via novelty detection on harmonic and textural features [H] |
| No dynamics (often) | Velocity is present but may not reflect compositional dynamics | Velocity treated as relative dynamics; absolute dynamic levels are not inferred |
| Quantisation noise | Performed MIDI has timing imprecision | Quantisation to the nearest grid value at a configurable resolution |
| No articulation labels | Duration and velocity encode articulation implicitly | Articulation inference from note duration relative to inter-onset interval [H] |

#### 1.3.1 MIDI Ingestion Pipeline

**Stage 1: Parse.** Read the MIDI file. Extract tracks, tempo map, time signature events (if present), note-on/note-off pairs with timing and velocity.

**Stage 2: Quantise.** Snap note onsets and durations to the nearest rational grid value. The grid resolution is configurable (default: 32nd note). Quantisation uses a minimum-distance algorithm that considers the global tempo and any time signature metadata.

**Stage 3: Assign measures.** Using tempo and time signature metadata (if present) or inferred metre, divide the quantised note stream into measures. Each measure receives a bar number and a time signature.

**Stage 4: Assign voices.** Within each measure, separate polyphonic note streams into distinct voices using a voice-separation algorithm. For keyboard music, the standard heuristic is to separate by register (above/below a split point, typically C4) and then by temporal overlap within each register. For more complex textures, a minimum-crossing algorithm assigns notes to voices such that voice crossings are minimised. [H]

**Stage 5: Infer key.** Estimate the key signature for each section using pitch-class distribution correlation against major and minor key profiles (Krumhansl-Schmuckler, Temperley, or Albrecht-Shanahan). Key changes are detected by windowed analysis with a configurable window size (default: 4 measures). [H]

**Stage 6: Spell pitches.** Convert MIDI note numbers to SpelledPitch values using the inferred key context and melodic interval heuristics. Within a diatonic context, prefer spellings that minimise accidentals. For chromatic passages, prefer spellings that produce conventional interval names (augmented sixth rather than minor seventh for the German sixth chord, for example). [H]

**Stage 7: Construct Score IR.** Assemble the processed data into a Score IR document with the appropriate structure: Score → Parts → Measures → Voices → Events. Attach the inferred key signature map and time signature map. Mark the analytical confidence for each inferred property.

**Stage 8: Validate.** Run Score IR validation. Flag any measures where voice durations do not fill the measure (quantisation artefacts) and apply corrective padding.

#### 1.3.2 Ingestion Confidence

Each ingested Score IR carries an *IngestionConfidence* record:

| Field | Type | Description |
|-------|------|-------------|
| `key_confidence` | `f32` | Confidence in key estimation (0.0–1.0) |
| `metre_confidence` | `f32` | Confidence in metre/barline placement |
| `spelling_confidence` | `f32` | Confidence in enharmonic spelling |
| `voice_separation_confidence` | `f32` | Confidence in voice assignment |
| `quantisation_residual` | `f32` | Mean timing error introduced by quantisation (in ms) |
| `source_format` | `String` | Original format |
| `manual_corrections` | `Vec<ManualCorrection>` | Any human corrections applied post-ingestion |

For MusicXML ingestion, key_confidence, metre_confidence, and spelling_confidence are 1.0 (these properties are explicit in the format). Voice separation confidence may still be less than 1.0 if the MusicXML encodes multiple voices ambiguously.

### 1.4 MusicXML Ingestion

MusicXML provides explicit key signatures, time signatures, enharmonic spellings, clefs, dynamics, articulations, and lyrics. The ingestion pipeline is a direct structural mapping from MusicXML elements to Score IR types (the inverse of the Score IR's MusicXmlCompiler, §9.6 of the Score IR Spec).

| MusicXML Element | Score IR Type |
|-----------------|---------------|
| `<score-partwise>` | Score |
| `<part>` | Part |
| `<measure>` | Measure |
| `<note>` | Note within NoteGroup |
| `<rest/>` | Rest |
| `<pitch>` | SpelledPitch |
| `<key>` | KeySignature |
| `<time>` | TimeSignature |
| `<direction>` with tempo | TempoMap entry |
| `<dynamics>` | Dynamic marking |
| `<wedge>` | Hairpin |
| `<articulations>` | Articulation |
| `<slur>` | Slur annotation |
| `<tied>` | Tie |

MusicXML ingestion is high-fidelity but still requires post-processing: the theory engine performs harmonic analysis, formal segmentation, and voice-leading analysis on the imported score. These derived analytical layers are not present in MusicXML and must be generated.

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

Once a work has been ingested into Score IR form, the Corpus IR performs a systematic analytical decomposition using the theory engine. This decomposition extracts every analytically significant property of the work at every structural level: individual notes, intervals, chords, phrases, sections, and the work as a whole. The result is a *WorkAnalysis* — a comprehensive, queryable analytical record.

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
| `annotations` | `Vec<HarmonicAnnotation>` | From the Score IR's HarmonicAnnotationLayer |
| `chord_vocabulary` | `Map<ChordQuality, u32>` | Frequency count of each chord quality |
| `progression_inventory` | `Vec<ProgressionPattern>` | Catalogued chord progressions |
| `modulation_inventory` | `Vec<ModulationEvent>` | Key changes with technique classification |
| `harmonic_rhythm` | `HarmonicRhythmProfile` | Distribution of harmonic change rates |
| `cadence_inventory` | `Vec<CadenceEvent>` | All cadences with type and position |
| `chromatic_techniques` | `Vec<ChromaticEvent>` | Secondary dominants, augmented sixths, Neapolitans, common-tone diminished, etc. |
| `tonicisation_frequency` | `Map<ScaleDegree, u32>` | How often each scale degree is tonicised |
| `tonal_plan` | `TonalPlan` | Sequence of key areas with durations |

#### 2.3.1 ProgressionPattern

| Field | Type | Description |
|-------|------|-------------|
| `roman_numerals` | `Vec<String>` | Sequence of Roman numerals (e.g., ["I", "IV", "V7", "I"]) |
| `length` | `u8` | Number of chords in the pattern |
| `occurrences` | `Vec<ScoreTime>` | Where this pattern appears in the score |
| `key_context` | `KeySignature` | Key in which this pattern occurs |

#### 2.3.2 ModulationEvent

| Field | Type | Description |
|-------|------|-------------|
| `position` | `ScoreTime` | Where the modulation occurs |
| `from_key` | `KeySignature` | Key before modulation |
| `to_key` | `KeySignature` | Key after modulation |
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
| `range` | `(SpelledPitch, SpelledPitch)` | Lowest and highest pitch |
| `tessitura` | `(SpelledPitch, SpelledPitch)` | Most frequently occupied register |
| `interval_distribution` | `Map<Interval, u32>` | Frequency of each melodic interval |
| `contour_inventory` | `Vec<ContourSegment>` | Catalogued melodic contour shapes |
| `scale_degree_distribution` | `Map<u8, u32>` | Frequency of each scale degree (1–7, chromatic) |
| `leap_resolution_rate` | `f32` | Proportion of leaps (> M2) followed by stepwise motion |
| `conjunct_proportion` | `f32` | Proportion of stepwise intervals |
| `longest_ascending_run` | `u8` | Longest consecutive ascending stepwise motion |
| `longest_descending_run` | `u8` | Longest consecutive descending stepwise motion |
| `chromaticism_rate` | `f32` | Proportion of non-diatonic pitches |

#### 2.4.2 ContourSegment

| Field | Type | Description |
|-------|------|-------------|
| `start` | `ScoreTime` | Beginning of the contour segment |
| `end` | `ScoreTime` | End of the contour segment |
| `shape` | `ContourShape` | Classification |
| `pitch_range` | `Interval` | Span of the segment |
| `duration` | `Beat` | Temporal span |
| `peak_position` | `f32` | Relative position of the highest pitch (0.0 = start, 1.0 = end) |
| `nadir_position` | `f32` | Relative position of the lowest pitch |

**ContourShape**: `Ascending`, `Descending`, `Arch` (ascends then descends), `InvertedArch`, `Stationary`, `Oscillating`, `AscendingArch`, `DescendingArch`, `Complex`.

#### 2.4.3 ThematicUnit

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<ThematicUnit>` | Unique identifier |
| `label` | `String` | e.g., "First theme", "Second theme", "Closing theme", "Motto" |
| `pitches` | `Vec<SpelledPitch>` | Pitch sequence |
| `intervals` | `Vec<Interval>` | Interval sequence (more stable across transpositions) |
| `rhythm` | `Vec<Beat>` | Duration sequence |
| `contour` | `Vec<i8>` | Contour reduction (−1, 0, +1 for descending, same, ascending) |
| `occurrences` | `Vec<ThematicOccurrence>` | Where this theme appears |

**ThematicOccurrence**:

| Field | Type | Description |
|-------|------|-------------|
| `position` | `ScoreTime` | Location |
| `part_id` | `Id<Part>` | Which voice carries it |
| `transformation` | `ThematicTransformation` | How it relates to the original statement |
| `key` | `KeySignature` | Key of this statement |

**ThematicTransformation**: `Original`, `TransposedExact`, `TransposedTonal`, `Inverted`, `Retrograde`, `RetrogradeInversion`, `Augmented`, `Diminished`, `Fragmented`, `Extended`, `SequentialRepetition`, `Ornamented`, `Simplified`, `Developed`.

### 2.5 Rhythmic Analysis Record

**Definition 2.5.1** [C]. The *RhythmicAnalysisRecord* analyses rhythmic properties.

| Field | Type | Description |
|-------|------|-------------|
| `duration_distribution` | `Map<Beat, u32>` | Frequency of each note duration |
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
| `key_sequence` | `Vec<(String, KeySignature, u32)>` | (Section label, key, duration in bars) |
| `key_area_count` | `u32` | Number of distinct key areas visited |
| `most_distant_key` | `KeySignature` | Key most distant from the tonic on the circle of fifths |
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
| `register_span_curve` | `Vec<(ScoreTime, Interval)>` | Vertical span (lowest to highest pitch) over time |
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
| `colour_changes` | `Vec<ColourChange>` | Points where orchestration changes dramatically |
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
| `dynamic_arc` | `Vec<(f32, DynamicLevel)>` | Relative position → dynamic |
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
| `chord_frequency` | `Map<ChordQuality, f32>` | Normalised frequency of each chord quality |
| `preferred_progressions` | `Vec<RankedProgression>` | Most frequent chord progressions |
| `modulation_frequency` | `f32` | Average modulations per work |
| `modulation_technique_preference` | `Map<ModulationTechnique, f32>` | Distribution of modulation techniques |
| `preferred_key_relationships` | `Map<Interval, f32>` | Distribution of tonal relationships between sections |
| `chromatic_density` | `f32` | Average proportion of chromatic chords per work |
| `secondary_dominant_frequency` | `f32` | How often secondary dominants appear |
| `augmented_sixth_frequency` | `f32` | How often augmented sixth chords appear |
| `neapolitan_frequency` | `f32` | How often Neapolitan chords appear |
| `harmonic_rhythm_mean` | `f32` | Average harmonic changes per bar |
| `harmonic_rhythm_variance` | `f32` | Variability of harmonic rhythm |
| `cadence_type_distribution` | `Map<CadenceType, f32>` | Preference for cadence types |
| `deceptive_cadence_frequency` | `f32` | How often expected cadences are evaded |
| `preferred_key_signatures` | `Map<KeySignature, u32>` | Preferred keys across the corpus |
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
| `duration_distribution` | `Map<Beat, f32>` | Normalised note duration distribution |
| `preferred_durations` | `Vec<Beat>` | Most frequent note values |
| `syncopation_index` | `f32` | Average syncopation across works |
| `rhythmic_variety` | `f32` | Entropy of the duration distribution |
| `preferred_metres` | `Map<TimeSignature, u32>` | Distribution of time signatures |
| `metrical_complexity` | `f32` | Frequency of irregular metres and changes |
| `tempo_preference` | `(f32, f32)` | Mean and standard deviation of preferred tempos |
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
| `density_range` | `(f32, f32)` | Minimum and maximum typical density |
| `texture_type_distribution` | `Map<String, f32>` | Preference for monophonic/homophonic/polyphonic/etc. |
| `register_span_preference` | `f32` | Average vertical span in semitones |
| `density_dynamic_correlation` | `f32` | Correlation between textural density and dynamic level |

### 3.11 Dynamic Style Profile

| Field | Type | Description |
|-------|------|-------------|
| `dynamic_range_preference` | `(DynamicLevel, DynamicLevel)` | Typical extremes |
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

**Definition 3.14.1**. A *SignaturePattern* is a distinctive musical fingerprint — a pattern so characteristic of this composer that its presence is a strong stylistic signal. These are extracted by comparing the composer's patterns against the corpus average and identifying statistically significant deviations.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<SignaturePattern>` | Unique identifier |
| `description` | `String` | Human-readable description |
| `domain` | `String` | Which analytical domain (harmonic, melodic, rhythmic, etc.) |
| `pattern_data` | `PatternData` | The pattern itself |
| `distinctiveness` | `f32` | How much more frequent this pattern is for this composer than for others (ratio or z-score) |
| `examples` | `Vec<(Id<IngestedWork>, ScoreTime)>` | Specific instances |

**PatternData**: A sum type covering different kinds of patterns:

| Variant | Description | Example |
|---------|-------------|---------|
| `HarmonicProgression(Vec<String>)` | Chord progression | Chopin's "♭VI → V" approach to cadences |
| `MelodicCell(Vec<Interval>)` | Interval sequence | Beethoven's short-short-short-long motto |
| `RhythmicFigure(Vec<Beat>)` | Duration pattern | Dotted rhythm prevalence in French overture style |
| `CadentialApproach(Vec<String>)` | How cadences are prepared | Bach's characteristic pre-cadential patterns |
| `TexturalGesture(String)` | Described texture | Debussy's parallel chord planing |
| `OrchestrationalSignature(Vec<String>)` | Instrument combination | Ravel's use of celesta + harp + muted strings |
| `FormalProportion(Vec<f32>)` | Section length ratios | Haydn's compact development sections |
| `RegistralSignature(String)` | Register usage | Liszt's extreme register contrasts |
| `DynamicGesture(String)` | Dynamic pattern | Schubert's pianissimo codas |

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

**Definition 5.2.1**. The *HowWouldXHandle* query is the highest-level query in the Corpus IR. It accepts a natural-language description of a compositional situation and returns the most relevant insights from the composer's profile and corpus.

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

The query matches the situation description against the formal, harmonic, textural, and motivic analysis records to find relevant passages in the corpus. It returns:

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
| `relevance_score` | `f32` | How closely this example matches the query (0.0–1.0) |
| `analysis_summary` | `String` | Concise analytical description of what happens in this passage |
| `harmonic_reduction` | `Vec<String>` | Roman numeral analysis of the passage |
| `formal_context` | `String` | Where in the formal structure this passage occurs |

**Tendency**:

| Field | Type | Description |
|-------|------|-------------|
| `domain` | `String` | Analytical domain |
| `observation` | `String` | e.g., "In development sections, this composer increases harmonic rhythm by 40% relative to exposition" |
| `confidence` | `f32` | Based on sample size |
| `supporting_examples_count` | `u32` | How many examples support this tendency |

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
| `ingest_batch` | Ingest a directory of files |
| `create_composer_profile` | Create a new ComposerProfile |
| `assign_work_to_composer` | Associate an ingested work with a composer |
| `assign_work_to_period` | Assign a work to a compositional period within a composer's profile |
| `set_work_metadata` | Update metadata for an ingested work |

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

### 6.2 Complete Ingestion Workflow

1. **Collect**: Gather MIDI or MusicXML files for a composer.
2. **Create profile**: `create_composer_profile("Sergei Rachmaninov", birth_year=1873, death_year=1943)`.
3. **Ingest**: `ingest_batch("/corpus/rachmaninov/", format="midi")`. Each file is processed through the ingestion pipeline (§1).
4. **Assign**: `assign_work_to_composer(work_id, composer_id)` for each ingested work. Set metadata: title, opus, year, instrumentation, whether it is a reduction.
5. **Assign periods** (optional): `assign_work_to_period(work_id, "Early")`, etc.
6. **Analyse**: `analyze_work(work_id)` for each work. This runs the full analytical decomposition (§2) using the theory engine.
7. **Build profile**: `rebuild_style_profile(composer_id)`. This aggregates all per-work analyses into the StyleProfile (§3).
8. **Detect signatures**: `detect_signature_patterns(composer_id)`. This identifies statistically distinctive patterns by comparing against the corpus mean.
9. **Query**: The profile is now available for composition-time queries.

### 6.3 Composition Workflow Integration

During composition, the agent interleaves production operations (Score IR, Timbre IR, Mix IR) with Corpus IR queries:

1. Agent is composing a piano concerto in the style informed by Rachmaninov.
2. Agent queries: `get_formal_template(composer_id, "Concerto")` → receives typical formal proportions, tonal plan, section characteristics.
3. Agent creates the Score IR: `create_score` with formal plan based on the template.
4. Agent queries: `query_how_would_x_handle(composer_id, "Opening of the first movement, establishing the main theme in solo piano before orchestral entry")` → receives examples from Rachmaninov's concerto openings, statistical tendencies for phrase lengths, typical key and dynamic profile.
5. Agent writes the opening melody: `write_melody` into the piano part, drawing on the melodic style profile (interval distribution, contour preferences, chromaticism rate).
6. Agent queries: `get_progression_examples(composer_id, ["i", "iv6", "V7", "i"])` → confirms this is within the harmonic vocabulary and finds examples of how Rachmaninov typically continues after this progression.
7. Agent continues composing, querying the Corpus IR whenever a creative decision would benefit from historical grounding.

The Corpus IR does not make the decisions. It informs them. The agent remains the creative agent; the corpus provides the education.

---

## 7. Storage

### 7.1 Corpus Database

The Corpus IR is stored as a structured database, not as a single document. The data volumes can be substantial: a single fully analysed orchestral score may generate thousands of analytical entries, and a corpus of hundreds of works produces millions of data points.

**Storage components**:

| Component | Format | Description |
|-----------|--------|-------------|
| Ingested Score IRs | JSON or binary Score IR format | Full Score IR for each ingested work |
| Work Analyses | JSON | Full analytical decomposition per work |
| Composer Profiles | JSON | Style profiles with aggregated statistics |
| Pattern Index | Indexed database (SQLite or similar) | Searchable index of all extracted patterns |
| Signature Patterns | JSON | Per-composer distinctive patterns |
| Comparison Cache | JSON | Cached StyleComparison results |

### 7.2 Incremental Updates

When a new work is ingested and analysed:
1. The work's analysis is stored.
2. The composer's StyleProfile is incrementally updated (distributions are recomputed with the additional data points, not rebuilt from scratch, unless `rebuild_style_profile` is explicitly called).
3. Signature patterns are marked as potentially stale (the additional data may change distinctiveness scores).
4. Cached comparisons involving this composer are invalidated.

---

## 8. Validation

### 8.1 Ingestion Validation

| Rule | Severity | Description |
|------|----------|-------------|
| C1 | Warning | Ingestion confidence below 0.7 in any dimension |
| C2 | Error | Ingested Score IR fails structural validation |
| C3 | Warning | Key estimation confidence below 0.5 (key may be incorrect) |
| C4 | Info | Work has no time signature metadata (inferred from content) |
| C5 | Warning | Voice separation produced more than 6 voices (possible error) |

### 8.2 Analysis Validation

| Rule | Severity | Description |
|------|----------|-------------|
| C6 | Error | Harmonic analysis coverage is less than 80% of the work |
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

The full MCP tool set across all five specifications:

| Layer | Tools | Examples |
|-------|-------|---------|
| Theory | ~7 | `analyze_harmony`, `voice_lead`, `generate_scale` |
| Score IR | ~25 | `create_score`, `write_melody`, `reorchestrate` |
| Timbre IR | ~18 | `set_sound_source`, `search_presets`, `compile_timbre` |
| Mix IR | ~24 | `set_channel_level`, `compare_to_reference`, `compile_mix` |
| Corpus IR | ~16 | `ingest_midi`, `query_style_profile`, `query_how_would_x_handle` |

Total: approximately 90 MCP tools providing an agent with complete control over music analysis, composition, arrangement, sound design, mixing, and mastering, informed by structured knowledge of the compositional repertoire.

---

## 10. Invariants

### 10.1 Structural

1. Every IngestedWork has a valid Score IR that passes structural validation.
2. Every IngestedWork is assigned to exactly one ComposerProfile.
3. Every ComposerProfile has at least one IngestedWork.
4. StyleProfiles are recomputable from the underlying WorkAnalysis records (no information is in the profile that is not derivable from the analyses).

### 10.2 Analytical

5. Harmonic analysis covers at least 80% of each ingested work.
6. Formal analysis produces a non-empty section plan for every work longer than 8 bars.
7. Statistical summaries in StyleProfiles report sample sizes alongside all aggregate values.

### 10.3 Consistency

8. If an IngestedWork is removed from the corpus, the composer's StyleProfile is updated to reflect the removal.
9. Period assignments are non-overlapping: a work belongs to at most one PeriodProfile.
10. Signature patterns are recomputed when the corpus changes (explicitly, not automatically).

---

*End of specification.*
