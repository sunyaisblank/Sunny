# Sunny Timbre IR — Formal Specification

**Version:** 0.1.0-draft  
**Date:** 2026-02-08  
**Status:** Initial specification; subject to iterative refinement  
**Dependencies:** Sunny Engine Formal Specification v0.1.0 (the "Theory Spec"); Sunny Score IR Specification v0.1.0 (the "Score IR Spec")

---

## 0. Preamble

### 0.1 Purpose

This document defines the formal specification for the Sunny Timbre IR, a structured representation of sound design and timbral control within the Sunny system. The Timbre IR models the sonic identity of each instrument in a production — its synthesis architecture, its spectral character, its temporal envelope behaviour, and its signal processing chain — as a semantically meaningful, queryable, and manipulable object.

The Timbre IR occupies a distinct architectural layer from both the theory engine and the Score IR. The theory engine defines the abstract musical structures (pitches, chords, rhythms). The Score IR instantiates those structures into a specific work with specific instruments and notation. The Timbre IR defines *how each instrument sounds*: the parameters that shape the waveform, the filters that sculpt the spectrum, the envelopes that control the temporal evolution, and the effects that colour the output. The Score IR says "Violin I plays A4 mezzo-forte with vibrato"; the Timbre IR says "the violin patch uses a multi-sampled source with expression-controlled crossfade between non-vibrato and vibrato layers, routed through a convolution reverb modelling a concert hall early reflection pattern."

### 0.2 Scope

The Timbre IR covers every parameter that shapes the tonal quality of a sound source, from oscillator configuration through the per-instrument effects chain. It does not cover mix-level processing (bus compression, master EQ, spatial positioning across the stereo or surround field), which is the domain of the Mix IR specification. The boundary is: the Timbre IR controls what comes out of an individual instrument channel *before* it enters the mixing stage.

For acoustic instruments rendered through sample libraries, the Timbre IR models the library's parameter space: articulation layers, dynamic layers, round-robin configuration, microphone position selection, and any built-in processing. For synthesised instruments, it models the synthesis architecture directly. For hybrid instruments (sample + synthesis), it models both layers and their interaction.

### 0.3 Design Objectives

1. **Semantic abstraction over raw parameters**: The Timbre IR represents sound design intent, not just parameter values. "Warm pad with slow attack and moderate brightness" is a semantic description that maps to a constellation of parameter values; the Timbre IR captures both the intent and the values, so that an agent can reason about timbre at the appropriate abstraction level.

2. **Instrument-agnostic structure**: The Timbre IR defines a universal structure for describing any sound source — whether it is a Steinway piano sample library, a Moog-style subtractive synthesiser, an FM engine, a granular processor, or a field recording. Instrument-specific details are captured through typed parameter sets within the universal structure.

3. **Correspondence to the Score IR**: Every Part in the Score IR has a corresponding TimbreProfile in the Timbre IR. The two are linked by Part ID. Changes to the Timbre IR do not affect the Score IR (the notes do not change); changes to the Score IR's Part assignments may require Timbre IR updates (if a part is reassigned to a different instrument).

4. **Compilation to DAW state**: The Timbre IR compiles to Ableton device chains, plugin parameter states, and automation curves via the LOM bridge, deterministically and reproducibly.

### 0.4 Notation Conventions

Inherits the conventions of the Theory Spec and Score IR Spec. Additional conventions:

- Parameter values are normalised to [0.0, 1.0] unless a physical unit is specified. Mappings to physical units (Hz, dB, ms) are defined per parameter.
- Signal flow is represented as a directed acyclic graph (DAG) from source to output.
- Frequency values in the audio domain use **Hz** (hertz). Filter frequencies, oscillator frequencies, and LFO rates are all in Hz unless otherwise noted.
- Time values in the audio domain use **seconds** (s) or **milliseconds** (ms). Envelope stages, delay times, and modulation rates are in these units.
- Amplitude values use **dB** (decibels) relative to full scale (dBFS) for levels, or linear gain factors where specified.

---

## 1. Architecture

### 1.1 TimbreProfile

**Definition 1.1.1**. A *TimbreProfile* is the root object of a single instrument's timbral configuration. Each Part in the Score IR has exactly one associated TimbreProfile.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<TimbreProfile>` | Unique identifier |
| `part_id` | `Id<Part>` | Corresponding Score IR part |
| `name` | `String` | Descriptive name (e.g., "Warm Analog Pad", "Spitfire Chamber Strings Vln I") |
| `source` | `SoundSource` | The sound generation engine |
| `insert_chain` | `EffectChain` | Per-instrument effects processing |
| `semantic_descriptors` | `SemanticTimbreDescriptor` | High-level timbral characterisation |
| `parameter_automation` | `Vec<TimbreAutomation>` | Time-varying parameter changes |
| `presets` | `Vec<TimbrePreset>` | Named parameter snapshots |
| `rendering` | `TimbreRenderingConfig` | DAW-specific device/plugin mapping |

### 1.2 Signal Flow

The signal flow within a TimbreProfile is a linear chain:

```
SoundSource → InsertChain → [output to Mix IR channel]
```

The SoundSource produces the raw audio signal. The InsertChain processes it through a sequence of effects. The output feeds into the Mix IR's channel strip for the corresponding part.

Within the SoundSource itself, the signal flow may be more complex (multiple oscillators, FM operators, sample layers), modelled as an internal DAG.

---

## 2. Sound Source

### 2.1 SoundSource

**Definition 2.1.1**. A *SoundSource* is the sound generation engine. It is a sum type with one variant per synthesis paradigm.

```
SoundSource
 ├── SubtractiveSynth
 ├── FMSynth
 ├── WavetableSynth
 ├── GranularSynth
 ├── AdditiveSynth
 ├── PhysicalModel
 ├── Sampler
 └── Hybrid { layers: Vec<SoundSource>, routing: LayerRouting }
```

Each variant contains the parameters specific to its synthesis method. The `Hybrid` variant allows combining multiple sources (e.g., a sample layer blended with a synthesised sub-bass layer), with routing rules specifying how the layers combine (mix, crossfade, velocity-split, key-split).

### 2.2 SubtractiveSynth

**Definition 2.2.1**. A *SubtractiveSynth* models the classic analogue synthesis architecture: oscillators → mixer → filter → amplifier, with modulation sources.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `oscillators` | `Vec<Oscillator>` | One or more oscillators (typically 2–3) |
| `noise` | `Option<NoiseGenerator>` | White/pink/brown noise source |
| `oscillator_mix` | `Vec<f32>` | Level of each oscillator in the pre-filter mix (linear gain) |
| `filter` | `Filter` | Primary filter |
| `filter_2` | `Option<Filter>` | Optional secondary filter (serial or parallel) |
| `filter_routing` | `FilterRouting` | `Serial` or `Parallel` if two filters |
| `amplifier` | `AmplifierEnvelope` | Output amplitude envelope |
| `modulation_matrix` | `ModulationMatrix` | Modulation routing |
| `unison` | `Option<UnisonConfig>` | Voice stacking for thickness |
| `portamento` | `Option<PortamentoConfig>` | Pitch glide between notes |

#### 2.2.1 Oscillator

| Field | Type | Description |
|-------|------|-------------|
| `waveform` | `Waveform` | Basic waveform type |
| `tune_semitones` | `i8` | Coarse tuning offset (semitones) |
| `tune_cents` | `f32` | Fine tuning offset (cents, −100 to +100) |
| `phase` | `f32` | Starting phase (0.0–1.0) |
| `pulse_width` | `f32` | Pulse width for square/pulse waves (0.0–1.0; 0.5 = square) |
| `level` | `f32` | Output level (0.0–1.0) |

**Waveform** enumeration: `Sine`, `Triangle`, `Saw`, `Square`, `Pulse`, `SuperSaw { detune: f32, voice_count: u8 }`, `Custom { harmonics: Vec<(u16, f32, f32)> }` (partial number, amplitude, phase).

#### 2.2.2 NoiseGenerator

| Field | Type | Description |
|-------|------|-------------|
| `colour` | `NoiseColour` | `White`, `Pink`, `Brown`, `Blue`, `Violet` |
| `level` | `f32` | Mix level (0.0–1.0) |

#### 2.2.3 Filter

| Field | Type | Description |
|-------|------|-------------|
| `filter_type` | `FilterType` | Topology |
| `cutoff` | `f32` | Cutoff frequency in Hz |
| `resonance` | `f32` | Resonance / Q (0.0–1.0, where 1.0 = self-oscillation threshold) |
| `drive` | `f32` | Pre-filter saturation (0.0–1.0) |
| `key_tracking` | `f32` | Cutoff tracking with MIDI note (0.0 = none, 1.0 = full) |
| `envelope` | `Option<Envelope>` | Filter envelope modulating cutoff |
| `envelope_depth` | `f32` | Envelope modulation amount (−1.0 to 1.0) |

**FilterType**: `LowPass { slope: FilterSlope }`, `HighPass { slope: FilterSlope }`, `BandPass { bandwidth: f32 }`, `Notch { bandwidth: f32 }`, `Comb { feedback: f32 }`, `FormantVowel { vowel: FormantVowel }`, `LadderLP` (Moog-style 4-pole), `StateVariable`, `Diode`.

**FilterSlope**: `Pole1` (6 dB/oct), `Pole2` (12 dB/oct), `Pole3` (18 dB/oct), `Pole4` (24 dB/oct).

#### 2.2.4 UnisonConfig

| Field | Type | Description |
|-------|------|-------------|
| `voice_count` | `u8` | Number of stacked voices (1–16) |
| `detune` | `f32` | Spread in cents between voices |
| `stereo_spread` | `f32` | Panning spread of voices (0.0–1.0) |
| `blend` | `f32` | Mix balance between original and detuned voices |

#### 2.2.5 PortamentoConfig

| Field | Type | Description |
|-------|------|-------------|
| `time` | `f32` | Glide time in milliseconds |
| `mode` | `PortamentoMode` | `Always`, `Legato` (only when overlapping notes) |
| `curve` | `GlideCurve` | `Linear`, `Exponential`, `SCurve` |

### 2.3 FMSynth

**Definition 2.3.1**. An *FMSynth* models frequency modulation synthesis using a network of operators (oscillator + envelope pairs) connected in an algorithm (routing topology).

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `operators` | `Vec<FMOperator>` | Operator definitions (typically 4–6) |
| `algorithm` | `FMAlgorithm` | Routing topology |
| `feedback` | `f32` | Self-modulation amount for the feedback operator |

**FMOperator**:

| Field | Type | Description |
|-------|------|-------------|
| `ratio` | `f32` | Frequency ratio to the carrier (1.0 = unison; 2.0 = octave) |
| `fixed_frequency` | `Option<f32>` | Fixed frequency in Hz (overrides ratio if present) |
| `level` | `f32` | Output level / modulation depth (0.0–1.0) |
| `envelope` | `Envelope` | Amplitude envelope for this operator |
| `detune` | `f32` | Fine tuning offset in cents |
| `waveform` | `Waveform` | Operator waveform (typically sine) |

**FMAlgorithm**: Either a predefined algorithm number (compatible with DX7-style numbering, 1–32) or a custom routing matrix specifying which operators modulate which.

Custom routing: `Vec<(u8, u8, f32)>` — (modulator_index, carrier_index, depth), forming a DAG.

### 2.4 WavetableSynth

**Definition 2.4.1**. A *WavetableSynth* crossfades between frames in a wavetable (a sequence of single-cycle waveforms).

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `wavetable` | `WavetableRef` | Reference to the wavetable data (file path or built-in name) |
| `position` | `f32` | Current frame position in the wavetable (0.0–1.0) |
| `frame_count` | `u32` | Number of frames in the table |
| `interpolation` | `InterpolationMode` | `Linear`, `Cubic`, `Spectral` |
| `filter` | `Option<Filter>` | Post-wavetable filter |
| `amplifier` | `AmplifierEnvelope` | Output amplitude envelope |
| `modulation_matrix` | `ModulationMatrix` | Modulation routing (LFO or envelope → position is the signature modulation) |

### 2.5 GranularSynth

**Definition 2.5.1**. A *GranularSynth* produces sound by combining many short audio fragments (grains) extracted from a source buffer.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `source` | `AudioBufferRef` | Reference to the source audio |
| `grain_size` | `f32` | Grain duration in milliseconds (1–500) |
| `grain_density` | `f32` | Grains per second |
| `position` | `f32` | Playback position in the source (0.0–1.0) |
| `position_random` | `f32` | Random offset applied to each grain's position (0.0–1.0) |
| `pitch_random` | `f32` | Random pitch variation per grain (cents) |
| `grain_envelope` | `GrainEnvelope` | Shape of individual grain windows |
| `spray` | `f32` | Temporal scatter of grain onsets |
| `stereo_spread` | `f32` | Random panning of individual grains |
| `reverse_probability` | `f32` | Probability that a grain plays in reverse (0.0–1.0) |

**GrainEnvelope**: `Gaussian`, `Hann`, `Triangle`, `Trapezoid { attack_ratio: f32, release_ratio: f32 }`, `Rectangular`.

### 2.6 AdditiveSynth

**Definition 2.6.1**. An *AdditiveSynth* constructs timbre by individually controlling the amplitude and frequency of each harmonic partial.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `partial_count` | `u16` | Number of partials (up to 512) |
| `partials` | `Vec<PartialDefinition>` | Per-partial configuration |
| `global_envelope` | `AmplifierEnvelope` | Master amplitude envelope |

**PartialDefinition**:

| Field | Type | Description |
|-------|------|-------------|
| `ratio` | `f32` | Frequency ratio to fundamental (1.0, 2.0, 3.0 for harmonic; non-integer for inharmonic) |
| `amplitude` | `f32` | Relative amplitude (0.0–1.0) |
| `phase` | `f32` | Initial phase (0.0–1.0) |
| `envelope` | `Option<Envelope>` | Per-partial amplitude envelope |
| `detune` | `f32` | Fine tuning offset in cents |

### 2.7 PhysicalModel

**Definition 2.7.1**. A *PhysicalModel* simulates the physics of a vibrating system using waveguide, modal, or finite-element methods.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `model_type` | `PhysicalModelType` | The physical system being modelled |
| `exciter` | `ExciterConfig` | How the system is excited |
| `resonator` | `ResonatorConfig` | The vibrating body |
| `coupling` | `f32` | Exciter-to-resonator coupling strength |
| `damping` | `f32` | Energy loss rate |
| `brightness` | `f32` | Spectral tilt of the output |

**PhysicalModelType**: `String { material: StringMaterial }`, `Bar`, `Membrane`, `Tube { open_end: bool }`, `Plate`, `Reed { stiffness: f32 }`, `Lip { mass: f32 }`, `Bowed { bow_pressure: f32, bow_speed: f32 }`.

### 2.8 Sampler

**Definition 2.8.1**. A *Sampler* plays back recorded audio samples, optionally with multi-layer mapping for velocity, key range, and round-robin variation.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `library` | `String` | Sample library name (e.g., "Spitfire BBC Symphony Orchestra", "Kontakt Factory") |
| `preset` | `String` | Preset within the library |
| `sample_map` | `SampleMap` | Key/velocity/articulation mapping |
| `microphone_positions` | `Vec<MicrophonePosition>` | Available mic positions and their levels |
| `release_samples` | `bool` | Whether key-release samples are enabled |
| `round_robin_mode` | `RoundRobinMode` | `Sequential`, `Random`, `RoundRobin` |
| `envelope_override` | `Option<AmplifierEnvelope>` | Override the library's built-in ADSR |
| `filter` | `Option<Filter>` | Post-sample filter |
| `tuning_offset` | `f32` | Global tuning offset in cents |

**SampleMap**: A collection of zones, each mapping a key range × velocity range × articulation to a sample file. This is typically defined by the sample library and exposed as metadata; the Timbre IR references it structurally rather than redefining it.

**MicrophonePosition**:

| Field | Type | Description |
|-------|------|-------------|
| `name` | `String` | Position name (e.g., "Close", "Tree", "Ambient", "Outrigger") |
| `level` | `f32` | Mix level for this position (0.0–1.0) |
| `enabled` | `bool` | Whether this position is active |

### 2.9 Hybrid Source

**Definition 2.9.1**. A *Hybrid* source combines multiple SoundSource instances with a routing strategy.

**LayerRouting** variants:

| Variant | Description |
|---------|-------------|
| `Mix(Vec<f32>)` | All layers sound simultaneously at given levels |
| `VelocitySplit(Vec<(u8, u8)>)` | Each layer responds to a velocity range |
| `KeySplit(Vec<(SpelledPitch, SpelledPitch)>)` | Each layer responds to a key range |
| `Crossfade { parameter: ModulationSource, curve: CrossfadeCurve }` | Layers crossfade based on a modulation source |

---

## 3. Envelopes

### 3.1 Envelope

**Definition 3.1.1**. An *Envelope* defines a time-varying contour applied to a parameter. The Score IR uses the Theory Spec's Beat type for musical time; the Timbre IR uses physical time (milliseconds) because envelope behaviour is perceptual, not metrical.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `stages` | `Vec<EnvelopeStage>` | Ordered sequence of envelope stages |
| `loop` | `Option<EnvelopeLoop>` | Looping behaviour (for sustained sounds) |
| `velocity_sensitivity` | `f32` | How much velocity affects envelope depth (0.0–1.0) |
| `key_tracking` | `f32` | How much pitch affects envelope times (0.0–1.0; higher pitch = faster) |

**EnvelopeStage**:

| Field | Type | Description |
|-------|------|-------------|
| `duration` | `f32` | Time in milliseconds (0 for instantaneous) |
| `target_level` | `f32` | Level at the end of this stage (0.0–1.0) |
| `curve` | `EnvelopeCurve` | Shape of the transition |

**EnvelopeCurve**: `Linear`, `Exponential { curvature: f32 }`, `Logarithmic { curvature: f32 }`, `SCurve`, `Step` (instantaneous jump at stage boundary).

### 3.2 Standard ADSR

The classic Attack-Decay-Sustain-Release envelope is the most common configuration:

| Stage | Duration | Target Level | Notes |
|-------|----------|-------------|-------|
| Attack | configurable | 1.0 | Time from note-on to peak |
| Decay | configurable | sustain level | Time from peak to sustain |
| Sustain | (held) | configurable | Level while note is held |
| Release | configurable | 0.0 | Time from note-off to silence |

The ADSR is representable as a 3-stage envelope (Attack → peak, Decay → sustain, Release → zero) where the sustain stage is implicit (the envelope holds at the decay target until note-off).

### 3.3 AmplifierEnvelope

**Definition 3.3.1**. The *AmplifierEnvelope* is the envelope controlling the overall output level of the sound source. It is distinguished from other envelopes because it determines the audible duration of each note.

**Fields**: Same as Envelope, with the additional constraint that the final stage target level must be 0.0 (silence). The amplifier envelope is always triggered by note-on and released by note-off.

---

## 4. Modulation System

### 4.1 ModulationMatrix

**Definition 4.1.1**. The *ModulationMatrix* defines all modulation routings within a TimbreProfile. Each routing connects a modulation source to a target parameter with a specified depth.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `routings` | `Vec<ModulationRouting>` | All active modulation connections |

**ModulationRouting**:

| Field | Type | Description |
|-------|------|-------------|
| `source` | `ModulationSource` | Signal driving the modulation |
| `target` | `ModulationTarget` | Parameter being modulated |
| `depth` | `f32` | Modulation amount (−1.0 to 1.0; bipolar) |
| `via` | `Option<ModulationSource>` | Optional second source that scales the depth (for modulation of modulation) |

### 4.2 ModulationSource

**Definition 4.2.1**. Available modulation sources.

| Source | Description |
|--------|-------------|
| `LFO { index: u8 }` | Low-frequency oscillator |
| `Envelope { index: u8 }` | Envelope generator |
| `Velocity` | MIDI velocity of the triggering note |
| `KeyPosition` | MIDI note number (for key tracking) |
| `Aftertouch` | Channel or polyphonic aftertouch |
| `ModWheel` | CC 1 (modulation wheel) |
| `Expression` | CC 11 (expression pedal) |
| `BreathController` | CC 2 |
| `PitchBend` | Pitch bend wheel |
| `CC { number: u8 }` | Arbitrary MIDI CC |
| `Random` | Per-note random value |
| `StepSequencer { index: u8 }` | Internal step sequencer |
| `AudioFollower { sidechain: Id<Part> }` | Envelope follower on another part's audio |
| `MacroKnob { index: u8 }` | User-defined macro control |

### 4.3 ModulationTarget

**Definition 4.3.1**. Any numeric parameter within the TimbreProfile can be a modulation target. Targets are addressed by path:

Examples:
- `source.oscillators[0].tune_cents`
- `source.filter.cutoff`
- `source.filter.resonance`
- `source.wavetable.position`
- `insert_chain.effects[2].parameters.mix`
- `source.granular.position`
- `source.granular.grain_size`
- `source.amplifier.envelope.stages[0].duration` (attack time)

The path syntax mirrors the structural hierarchy of the TimbreProfile.

### 4.4 LFO

**Definition 4.4.1**. A *Low-Frequency Oscillator* produces a periodic control signal.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `waveform` | `LFOWaveform` | Shape of the oscillation |
| `rate` | `LFORate` | Speed of oscillation |
| `phase` | `f32` | Starting phase (0.0–1.0) |
| `symmetry` | `f32` | Skew / symmetry (0.5 = symmetric; applies to triangle, saw) |
| `retrigger` | `bool` | Reset phase on each note-on |
| `fade_in` | `f32` | Time in ms before LFO reaches full depth (0 = immediate) |
| `delay` | `f32` | Time in ms before LFO begins after note-on |

**LFOWaveform**: `Sine`, `Triangle`, `Saw`, `Square`, `SampleAndHold`, `SmoothRandom`, `Custom { points: Vec<(f32, f32)> }`.

**LFORate**: Either `Free { hz: f32 }` (unsynced) or `Synced { division: Beat }` (synced to tempo from the Score IR's TempoMap). Tempo-synced rates use the exact rational Beat type.

### 4.5 Step Sequencer

**Definition 4.5.1**. An internal *StepSequencer* produces a stepped control signal synchronised to tempo.

| Field | Type | Description |
|-------|------|-------------|
| `steps` | `Vec<f32>` | Step values (0.0–1.0), one per step |
| `step_duration` | `Beat` | Duration of each step (exact rational, tempo-synced) |
| `smooth` | `f32` | Glide between steps (0.0 = hard step, 1.0 = full interpolation) |
| `loop_mode` | `LoopMode` | `Forward`, `PingPong`, `OneShot` |

### 4.6 Macro Knobs

**Definition 4.6.1**. A *MacroKnob* is a user-defined control that maps a single value to multiple modulation targets. Macros allow an agent to control complex timbral changes with a single parameter.

| Field | Type | Description |
|-------|------|-------------|
| `index` | `u8` | Macro number (0–7, typical) |
| `name` | `String` | Descriptive name (e.g., "Brightness", "Movement", "Grit") |
| `value` | `f32` | Current value (0.0–1.0) |
| `mappings` | `Vec<MacroMapping>` | Parameters controlled by this macro |

**MacroMapping**:

| Field | Type | Description |
|-------|------|-------------|
| `target` | `ModulationTarget` | Target parameter path |
| `min` | `f32` | Target value when macro is at 0.0 |
| `max` | `f32` | Target value when macro is at 1.0 |
| `curve` | `MappingCurve` | Transfer function shape |

**MappingCurve**: `Linear`, `Exponential`, `Logarithmic`, `SCurve`, `ReverseSCurve`, `Custom { points: Vec<(f32, f32)> }`.

---

## 5. Per-Instrument Effect Chain

### 5.1 EffectChain

**Definition 5.1.1**. An *EffectChain* is an ordered sequence of effects processors applied to the sound source output. This is the per-instrument insert chain; it represents processing that is intrinsic to the instrument's identity (e.g., an amp simulator on a guitar, a chorus on a string pad) rather than mixing-stage processing (which belongs to the Mix IR).

| Field | Type | Description |
|-------|------|-------------|
| `effects` | `Vec<Effect>` | Ordered sequence; signal flows left to right |
| `bypass_all` | `bool` | Bypass the entire chain |

### 5.2 Effect

**Definition 5.2.1**. An *Effect* is a single signal processor in the chain.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<Effect>` | Unique identifier |
| `effect_type` | `EffectType` | The processing algorithm |
| `enabled` | `bool` | Whether this effect is active |
| `mix` | `f32` | Dry/wet blend (0.0 = fully dry, 1.0 = fully wet) |
| `parameters` | `EffectParameters` | Type-specific parameter set |

### 5.3 Effect Types

#### 5.3.1 Distortion and Saturation

**DistortionEffect**:

| Field | Type | Description |
|-------|------|-------------|
| `algorithm` | `DistortionAlgorithm` | Clipping / waveshaping model |
| `drive` | `f32` | Input gain / distortion amount (0.0–1.0) |
| `tone` | `f32` | Post-distortion EQ tilt (0.0 = dark, 1.0 = bright) |
| `output_level` | `f32` | Gain compensation |

**DistortionAlgorithm**: `SoftClip`, `HardClip`, `FoldBack`, `TubeEmulation`, `TapeEmulation`, `Bitcrush { bit_depth: u8, sample_rate_reduction: f32 }`, `WaveShaper { curve: Vec<(f32, f32)> }`, `RingModulation { frequency: f32 }`.

#### 5.3.2 Delay

**DelayEffect**:

| Field | Type | Description |
|-------|------|-------------|
| `delay_time` | `DelayTime` | Delay duration |
| `feedback` | `f32` | Feedback amount (0.0–1.0; >1.0 for self-oscillation if clamped) |
| `stereo_mode` | `StereoDelayMode` | Mono, stereo, or ping-pong |
| `filter_type` | `Option<Filter>` | Filter in the feedback path |
| `modulation_rate` | `f32` | Delay time modulation rate in Hz (for chorus-like effects) |
| `modulation_depth` | `f32` | Delay time modulation depth in ms |

**DelayTime**: `Free { ms: f32 }` or `Synced { division: Beat }`.

**StereoDelayMode**: `Mono`, `Stereo { offset: f32 }` (offset between L/R in ms or beats), `PingPong`.

#### 5.3.3 Reverb

**ReverbEffect**:

| Field | Type | Description |
|-------|------|-------------|
| `algorithm` | `ReverbAlgorithm` | Reverb model |
| `decay_time` | `f32` | RT60 in seconds |
| `pre_delay` | `f32` | Pre-delay in milliseconds |
| `damping` | `f32` | High-frequency decay rate (0.0 = none, 1.0 = fully damped) |
| `diffusion` | `f32` | Echo density (0.0–1.0) |
| `size` | `f32` | Room size parameter (0.0–1.0) |
| `early_reflections_level` | `f32` | Early reflections mix level |
| `eq_low_cut` | `f32` | Low-frequency attenuation in Hz |
| `eq_high_cut` | `f32` | High-frequency attenuation in Hz |

**ReverbAlgorithm**: `Algorithmic`, `Convolution { impulse: AudioBufferRef }`, `Plate`, `Spring`, `Chamber`, `Hall`, `Room`, `Shimmer { pitch_shift: f32 }`.

#### 5.3.4 Modulation Effects

**ChorusEffect**:

| Field | Type | Description |
|-------|------|-------------|
| `rate` | `f32` | Modulation rate in Hz |
| `depth` | `f32` | Modulation depth (0.0–1.0) |
| `voices` | `u8` | Number of chorus voices |
| `feedback` | `f32` | Feedback amount |
| `stereo_spread` | `f32` | Width of stereo effect |

**PhaserEffect**:

| Field | Type | Description |
|-------|------|-------------|
| `rate` | `f32` | Sweep rate in Hz |
| `depth` | `f32` | Sweep depth |
| `stages` | `u8` | Number of all-pass stages (2–24) |
| `feedback` | `f32` | Resonance / feedback |
| `center_frequency` | `f32` | Centre of the sweep range in Hz |

**FlangerEffect**:

| Field | Type | Description |
|-------|------|-------------|
| `rate` | `f32` | Sweep rate in Hz |
| `depth` | `f32` | Sweep depth |
| `feedback` | `f32` | Feedback (negative values for through-zero flanging) |
| `manual` | `f32` | Base delay time |

#### 5.3.5 EQ (Per-Instrument)

**EQEffect**:

| Field | Type | Description |
|-------|------|-------------|
| `bands` | `Vec<EQBand>` | EQ bands |

**EQBand**:

| Field | Type | Description |
|-------|------|-------------|
| `frequency` | `f32` | Centre/corner frequency in Hz |
| `gain` | `f32` | Gain in dB (−24 to +24) |
| `q` | `f32` | Bandwidth / Q factor |
| `band_type` | `EQBandType` | `Peak`, `LowShelf`, `HighShelf`, `LowCut`, `HighCut` |

#### 5.3.6 Dynamics (Per-Instrument)

**CompressorEffect**:

| Field | Type | Description |
|-------|------|-------------|
| `threshold` | `f32` | Threshold in dB |
| `ratio` | `f32` | Compression ratio (1.0 = no compression, ∞ = limiting) |
| `attack` | `f32` | Attack time in ms |
| `release` | `f32` | Release time in ms |
| `knee` | `f32` | Knee width in dB (0 = hard knee) |
| `makeup_gain` | `f32` | Output gain in dB |
| `sidechain` | `Option<SidechainConfig>` | External sidechain source |

**SidechainConfig**:

| Field | Type | Description |
|-------|------|-------------|
| `source` | `Id<Part>` | Part whose audio drives the sidechain |
| `filter` | `Option<Filter>` | Sidechain filter (e.g., high-pass to focus on kick drum) |

---

## 6. Semantic Timbral Descriptors

### 6.1 Purpose

Semantic descriptors allow an agent to reason about timbre at a perceptual level rather than at the parameter level. An agent composing an orchestral passage may need "a warm, dark string sound with a slow, singing quality" — this is a semantic specification that maps to a constellation of parameters (low-pass filter with moderate cutoff, slow attack, moderate vibrato depth, large reverb with high damping).

The descriptors are *annotations*, not *generators*: they describe the perceived character of the current parameter state, and they serve as search keys for preset selection and as targets for automated parameter tuning.

### 6.2 SemanticTimbreDescriptor

**Definition 6.2.1**.

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `brightness` | `f32` | 0.0–1.0 | Spectral centroid proxy; 0 = dark, 1 = bright |
| `warmth` | `f32` | 0.0–1.0 | Presence of lower harmonics; 0 = thin/cold, 1 = warm/rich |
| `roughness` | `f32` | 0.0–1.0 | Inharmonicity and beating; 0 = smooth, 1 = rough/gritty |
| `attack_character` | `AttackCharacter` | — | Percussive / gradual / bowed / plucked / etc. |
| `sustain_character` | `SustainCharacter` | — | Steady / evolving / decaying / pulsing |
| `width` | `f32` | 0.0–1.0 | Stereo width; 0 = mono, 1 = wide |
| `density` | `f32` | 0.0–1.0 | Spectral density; 0 = sparse/hollow, 1 = dense/full |
| `movement` | `f32` | 0.0–1.0 | Degree of temporal timbral variation; 0 = static, 1 = highly animated |
| `weight` | `f32` | 0.0–1.0 | Perceptual "heaviness"; 0 = light/airy, 1 = heavy/massive |
| `tags` | `Vec<String>` | — | Free-form descriptive tags (e.g., "glassy", "metallic", "woody", "ethereal") |

**AttackCharacter**: `Percussive`, `Plucked`, `Bowed`, `Blown`, `Gradual`, `Swelling`, `Explosive`.

**SustainCharacter**: `Steady`, `Decaying`, `Evolving`, `Pulsing`, `Swelling`, `Noisy`.

### 6.3 Descriptor Derivation

Semantic descriptors can be:
- **Manually assigned** by the user or agent.
- **Derived from parameters** [H] using heuristic rules (e.g., brightness correlates with filter cutoff relative to fundamental frequency and harmonic amplitude distribution).
- **Derived from audio analysis** [E] by analysing a rendered sample of the sound (spectral centroid, spectral flux, attack transient analysis).

The derivation mode is stored alongside the descriptor:

| Mode | Meaning |
|------|---------|
| `Manual` | Assigned by user/agent; not automatically updated |
| `ParameterDerived` | Computed from parameter state; updated when parameters change |
| `AudioDerived` | Computed from rendered audio; updated on explicit re-analysis |

---

## 7. Timbral Automation

### 7.1 TimbreAutomation

**Definition 7.1.1**. *TimbreAutomation* describes time-varying changes to Timbre IR parameters that are synchronised to the score timeline. These are distinct from modulation (which operates within the synthesis engine at audio rate) — timbral automation operates at the arrangement level, changing the character of an instrument across sections of the piece.

| Field | Type | Description |
|-------|------|-------------|
| `parameter_path` | `String` | Path to the automated parameter (same syntax as ModulationTarget) |
| `breakpoints` | `Vec<AutomationBreakpoint>` | Time-value pairs |
| `interpolation` | `InterpolationMode` | How values are interpolated between breakpoints |

**AutomationBreakpoint**:

| Field | Type | Description |
|-------|------|-------------|
| `time` | `ScoreTime` | Position in the score |
| `value` | `f32` | Parameter value at this time |

**InterpolationMode**: `Step` (hold until next breakpoint), `Linear`, `Smooth` (cubic Hermite), `Exponential`.

Timbral automation is the mechanism by which an agent can, for example, gradually open a filter across the build section of an electronic track, or switch a string section from a sustain articulation to a tremolo articulation at a specific bar.

### 7.2 Preset Morphing

**Definition 7.2.1**. A *PresetMorph* interpolates between two TimbrePresets over a score time range.

| Field | Type | Description |
|-------|------|-------------|
| `from_preset` | `Id<TimbrePreset>` | Starting preset |
| `to_preset` | `Id<TimbrePreset>` | Target preset |
| `start` | `ScoreTime` | Morph start position |
| `end` | `ScoreTime` | Morph end position |
| `curve` | `MappingCurve` | Interpolation curve |

All numeric parameters are interpolated; discrete parameters (waveform type, filter type) switch at the midpoint unless explicitly sequenced.

---

## 8. Presets

### 8.1 TimbrePreset

**Definition 8.1.1**. A *TimbrePreset* is a named snapshot of all parameter values within a TimbreProfile.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<TimbrePreset>` | Unique identifier |
| `name` | `String` | Display name |
| `description` | `Option<String>` | Human-readable description |
| `semantic_descriptors` | `SemanticTimbreDescriptor` | Perceptual characterisation |
| `parameter_state` | `Map<String, f32>` | Complete map of parameter path → value |
| `tags` | `Vec<String>` | Searchable tags (e.g., "bass", "lead", "pad", "orchestral", "vintage") |

### 8.2 Preset Library

A preset library is a collection of TimbrePresets, searchable by:
- Semantic descriptor ranges (e.g., brightness > 0.7 AND warmth > 0.5).
- Tags (e.g., "pad" AND "evolving").
- Instrument class compatibility (e.g., presets for SubtractiveSynth sources).
- Name or description text search.

---

## 9. Compilation

### 9.1 Compilation to Ableton

The *TimbreCompiler* maps each TimbreProfile to an Ableton instrument device chain.

**Mapping strategy**:

For **Sampler**-type sources using established sample libraries (Kontakt, Spitfire, EastWest, etc.), the compiler:
1. Creates a MIDI track in Ableton.
2. Loads the specified plugin preset.
3. Applies microphone position levels via plugin parameters.
4. Inserts the effect chain as Ableton audio effects on the track.

For **synthesiser**-type sources, the compiler:
1. Creates a MIDI track.
2. If a matching Ableton instrument exists (Operator for FM, Wavetable for wavetable, Simpler for sampler, Analog for subtractive), loads it and maps parameters.
3. If no native match exists, loads a third-party VST/AU plugin and maps parameters.
4. Applies the effect chain.

The parameter mapping is defined in `TimbreRenderingConfig`:

| Field | Type | Description |
|-------|------|-------------|
| `device_type` | `DeviceType` | `NativeAbleton { device_name: String }` or `Plugin { format: PluginFormat, identifier: String }` |
| `parameter_map` | `Map<String, DeviceParameter>` | Timbre IR parameter path → device parameter address |
| `preset_path` | `Option<String>` | Path to a preset file to load as baseline |

**DeviceParameter**:

| Field | Type | Description |
|-------|------|-------------|
| `device_index` | `u8` | Index of the device in the chain |
| `parameter_name` | `String` | Parameter name or index within the device |
| `range` | `(f32, f32)` | Min/max in the device's native units |
| `curve` | `MappingCurve` | Transfer function from normalised [0,1] to device range |

### 9.2 Automation Compilation

TimbreAutomation breakpoints compile to Ableton automation lanes:

1. Each automated parameter maps to an automation lane on the corresponding track/device parameter.
2. Breakpoint times are converted from ScoreTime to TickTime (via the Score IR's temporal coordinate system).
3. Interpolation between breakpoints is rendered as a sequence of automation points at a configurable resolution (default: 1 point per tick for linear, 4 points per beat for smooth).

---

## 10. Validation

### 10.1 Structural Validation

| Rule | Severity | Description |
|------|----------|-------------|
| T1 | Error | Every Score IR Part has a corresponding TimbreProfile |
| T2 | Error | SoundSource type is valid and all required fields are present |
| T3 | Warning | Filter cutoff exceeds Nyquist frequency (inaudible) |
| T4 | Warning | Oscillator detune exceeds ±100 cents |
| T5 | Warning | FM feedback exceeds stability threshold |
| T6 | Error | Effect chain contains a cycle (should be impossible by structure) |
| T7 | Warning | Modulation routing targets a non-existent parameter path |
| T8 | Info | Semantic descriptors are stale (parameters changed since last derivation) |
| T9 | Warning | TimbreRenderingConfig has unmapped parameters |

### 10.2 Perceptual Validation [E]

These validations require audio rendering and analysis:

| Rule | Severity | Description |
|------|----------|-------------|
| P1 | Warning | Output level exceeds 0 dBFS (clipping risk) |
| P2 | Info | Spectral content above 18 kHz is significant (may alias) |
| P3 | Info | Attack time longer than the shortest note in the Score IR for this part |

---

## 11. Agent Interface

### 11.1 MCP Tools

| Tool | Description |
|------|-------------|
| `create_timbre_profile` | Create a new TimbreProfile for a Score IR Part |
| `set_sound_source` | Set or change the sound source type and parameters |
| `add_effect` | Add an effect to the insert chain |
| `remove_effect` | Remove an effect from the chain |
| `reorder_effects` | Change effect order |
| `set_parameter` | Set any parameter by path |
| `create_macro` | Create a macro knob with mappings |
| `set_macro` | Set a macro knob value |
| `add_modulation` | Add a modulation routing |
| `add_automation` | Add timbral automation |
| `set_semantic_descriptors` | Set or update semantic descriptors |
| `search_presets` | Search the preset library by descriptors, tags, or text |
| `load_preset` | Apply a preset to a TimbreProfile |
| `save_preset` | Save current state as a preset |
| `morph_presets` | Set up a preset morph over a score time range |
| `analyze_timbre` | Derive semantic descriptors from current parameters or rendered audio |
| `compile_timbre` | Compile timbre configuration to DAW devices |
| `validate_timbre` | Run validation |

---

## 12. Invariants

1. Every Score IR Part has exactly one TimbreProfile.
2. Signal flow within a TimbreProfile is acyclic.
3. All parameter values are within their declared ranges.
4. Modulation targets reference valid parameter paths.
5. Timbral automation breakpoints reference valid ScoreTime positions within the score.
6. Preset parameter states are complete (every parameter path has a value).
7. The TimbreCompiler produces exactly one device chain per TimbreProfile.

---

*End of specification.*
