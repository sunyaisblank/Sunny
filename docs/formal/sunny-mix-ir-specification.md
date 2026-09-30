# Sunny Mix IR — Formal Specification

**Version:** 0.2.0-draft

**Date:** 2026-08-29
**Status:** Normative model with an explicit implemented runtime profile
**Dependencies:** Sunny Engine Formal Specification v0.1.0 (the "Theory Spec"); Sunny Score IR Specification v0.2.0 (the "Score IR Spec"); Sunny Timbre IR Specification v0.1.0 (the "Timbre IR Spec")

---

## 0. Preamble

### 0.1 Purpose

This document defines the formal specification for the Sunny Mix IR, a structured representation of the mixing, spatial positioning, and mastering stages of audio production. The Mix IR models the signal routing, level balancing, spectral shaping, dynamics processing, spatial placement, and master bus processing that transform individual instrument channels into a cohesive, finished production.

The Mix IR is the final layer in the Sunny production stack. The theory engine provides musical knowledge. The Score IR defines what is played. The Timbre IR defines how each instrument sounds. The Mix IR defines how the instruments are blended, balanced, and positioned to form the finished mix.

### 0.2 Architectural Position

The signal flow across the four Sunny layers is:

```
Theory Engine → Score IR → Timbre IR → Mix IR → Master Output
                  (what)     (how each    (how they
                              sounds)      combine)
```

The Mix IR receives the output of each Timbre IR channel (post-insert chain) and routes it through a mixing infrastructure consisting of channel strips, auxiliary buses, group buses, and a master bus. Every processing stage within this infrastructure is represented in the Mix IR.

### 0.3 Design Objectives

1. **Relational modelling**: The Mix IR represents mixing decisions as *relationships between channels* (this instrument is louder than that one; these instruments share a reverb space; this group is compressed together) rather than as isolated absolute values. Relationships are more robust under revision: if the agent raises the overall level of the strings section, the internal balance within the section is preserved.

2. **Semantic intent preservation**: Every processing decision is annotated with its purpose. A high-pass filter at 80 Hz on a vocal channel is not just "EQ band 1: HPF 80 Hz" — it carries the semantic annotation "remove low-frequency rumble below the vocal's fundamental range." This allows an agent to understand *why* a processing choice was made and to make informed modifications.

3. **Reference-aware mixing**: The Mix IR supports comparison against caller-supplied reference profiles, providing targets for spectral balance, dynamic range, loudness, and spatial width that an agent can work toward. Deriving those profiles from recordings is reserved design space, not a current runtime analyser.

4. **Non-destructive and automation-capable model**: Processing and time-varying intent are represented without modifying source material. Whether a parameter can be deployed is target-dependent; the current Ableton target reports automation as unsupported.

5. **Deterministic compilation**: The Mix IR compiler emits operations only for verified target
   mappings and reports every requested capability or mapping gap it encounters. The existence of
   a writable LOM parameter is not by itself proof that it realises a Mix IR field. The compiler
   never substitutes a merely similar track, property, or method.

6. **Single coherent routing graph**: `ChannelStrip.group_assignment` is an exact mirror of
   `GroupBus.member_channels`, and a grouped `GroupBus.output` is an exact mirror of its parent's
   `member_groups`. Contradictory, missing, or duplicate mirror edges are invalid rather than being
   resolved by choosing one representation opportunistically.

### 0.4 Notation Conventions

Inherits conventions from all preceding specifications. Additional conventions:

- Signal levels use **dBFS** (decibels relative to full scale) for digital levels and **dBu** where analogue-modelled levels are relevant.
- **LUFS** (Loudness Units relative to Full Scale) for integrated loudness measurements per ITU-R BS.1770.
- Frequency values in Hz. Bandwidth in octaves or Q factor.
- Time constants (attack, release) in milliseconds.
- Spatial positions use a coordinate system defined in §6.

### 0.5 Implemented Runtime Profile

The current runtime implements the typed MixGraph model, including first-class mix-stage delay and reverberation, versioned JSON, static parameter/reference comparison, validation, and capability-aware Ableton mixer compilation. Reference measurements are supplied by the caller; Sunny does not analyse the referenced audio file. It does not render PCM audio, implement immersive rendering, or author Ableton automation envelopes. Unsupported target operations remain in the IR and make compilation incomplete with explicit warnings.

---

## 1. Signal Flow Graph

### 1.1 Overview

The Mix IR's signal flow is a directed acyclic graph (DAG) with four node types:

```
ChannelStrip[] ──→ GroupBus[] ──→ MasterBus ──→ Output
      │                │
      └──→ AuxSend[] ──┘
              │
         AuxReturn[]
              │
              └──→ GroupBus / MasterBus
```

**ChannelStrip**: One per Score IR Part. Receives the output of the Timbre IR's insert chain.

**GroupBus**: Combines multiple channels for collective processing (e.g., "All Strings", "Drum Bus", "Brass Section"). A channel can feed into at most one group bus (single-parent constraint); group buses can nest (a "Woodwinds" group within an "Orchestral" group).

**AuxSend/Return**: Parallel effects processing. A channel sends a copy of its signal (at a configurable level) to an auxiliary bus, which processes it (typically reverb or delay) and returns the processed signal to the mix.

**MasterBus**: The final summing point. All group buses, ungrouped channels, and aux returns converge here. The master bus carries the master processing chain and outputs the final stereo (or surround) signal.

### 1.2 MixGraph

**Definition 1.2.1**. The *MixGraph* is the root object of the Mix IR.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<MixGraph>` | Unique identifier |
| `channels` | `Vec<ChannelStrip>` | One per Score IR Part |
| `group_buses` | `Vec<GroupBus>` | Grouping and collective processing |
| `aux_sends` | `Vec<AuxBus>` | Parallel effects buses |
| `master_bus` | `MasterBus` | Master output chain |
| `mix_annotations` | `Vec<MixAnnotation>` | Semantic intent annotations |
| `reference_profiles` | `Vec<ReferenceProfile>` | Target mix characteristics |
| `automation` | `Vec<MixAutomation>` | Time-varying parameter changes |
| `output_format` | `OutputFormat` | Stereo, surround, immersive |

**OutputFormat**: `Stereo`, `LCR` (left, centre, right), `Quad`, `Surround51`, `Surround71`, `Atmos { bed_channels: u8, object_count: u8 }`, `Binaural`.

### 1.3 Signal Flow Validation

**Invariant**: The signal flow graph is a DAG. No signal path cycles are permitted. Validation checks:
- No channel routes to itself.
- No group bus routes to a group bus that is its own ancestor.
- Aux sends do not create feedback loops (an aux return does not feed back into the same aux bus's input).
- Every channel ultimately reaches the master bus through some path.

---

## 2. Channel Strip

### 2.1 ChannelStrip

**Definition 2.1.1**. A *ChannelStrip* represents the mixing controls for a single instrument channel.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<ChannelStrip>` | Unique identifier |
| `part_id` | `Id<Part>` | Corresponding Score IR Part |
| `input_trim` | `f32` | Input gain trim in dB (−24 to +24) |
| `polarity_invert` | `bool` | Phase inversion (180°) |
| `insert_chain` | `MixEffectChain` | Channel insert effects (EQ, compression, etc.) |
| `fader` | `Fader` | Channel level control |
| `spatial` | `SpatialPosition` | Pan / spatial placement |
| `mute` | `bool` | Channel mute |
| `solo` | `bool` | Channel solo |
| `sends` | `Vec<AuxSendLevel>` | Levels to each aux bus |
| `group_assignment` | `Option<Id<GroupBus>>` | Which group bus this channel feeds |
| `intent` | `Option<ChannelIntent>` | Semantic mixing intent |

### 2.2 Fader

**Definition 2.2.1**. The *Fader* is the primary level control.

| Field | Type | Description |
|-------|------|-------------|
| `level_db` | `f32` | Fader position in dB (−∞ to +12) |
| `relative_level` | `Option<RelativeLevel>` | Level expressed as a relationship to other channels |

**RelativeLevel**: Expresses the fader position as a relationship rather than an absolute value:

| Field | Type | Description |
|-------|------|-------------|
| `reference` | `LevelReference` | What this level is relative to |
| `offset_db` | `f32` | dB offset from the reference |

**LevelReference**: `MasterTarget { lufs: f32 }` (absolute target loudness), `Channel { id: Id<ChannelStrip>, relationship: String }` (e.g., "3 dB below Violin I"), `Group { id: Id<GroupBus> }` (relative to the group's internal balance).

Let the fader nodes be

\[
V = C \cup G \cup \{M\},
\]

where `C` is the set of channel faders, `G` the set of group faders, and `M` the master fader. Every
node has an explicit fallback value `e(v)`. A Channel or Group reference from `v` to `r` declares
the exact additive constraint

\[
L(v) = L(r) + \Delta_v,
\]

where `Δv = offset_db`; the free-form Channel `relationship` string is explanatory and cannot
override this equation. The reference graph must be closed and acyclic. Its unique statically
tractable solution is obtained by deterministic dependency-first evaluation. All explicit and
derived values must be finite and no greater than +12 dB.

A `MasterTarget` reference instead constrains measured programme loudness to `lufs + offset_db`.
There is no target-independent function from LUFS intent to a channel/group/master fader position:
the result depends on rendered signal energy, summation, and downstream nonlinear processing.
Accordingly it has status `requires_loudness_measurement`; any Channel/Group dependant has status
`blocked_by_unresolved_reference`. Both retain their explicit `level_db` fallback. Sunny never
pretends that LUFS and fader dB are interchangeable.

`resolve_relative_levels` returns one proof record for every channel, group, and master fader with
status `explicit`, `resolved`, `requires_loudness_measurement`, or
`blocked_by_unresolved_reference`. The Ableton compiler applies exact resolved values. It applies
the explicit fallback only for the two measurement-dependent states and reports each residual,
making compilation incomplete. Setting a channel's absolute level clears its prior relative
constraint; relative mutation is transactional and rejects missing references or cycles.

### 2.3 AuxSendLevel

| Field | Type | Description |
|-------|------|-------------|
| `aux_bus_id` | `Id<AuxBus>` | Target aux bus |
| `level_db` | `f32` | Send level in dB |
| `pre_fader` | `bool` | If true, send level is independent of fader; if false, send follows fader |
| `enabled` | `bool` | Send active/inactive |

### 2.4 ChannelIntent

**Definition 2.4.1**. Semantic annotation describing the mixing intent for this channel.

| Field | Type | Description |
|-------|------|-------------|
| `role_in_mix` | `MixRole` | The channel's function in the mix |
| `frequency_space` | `FrequencyAllocation` | Where this instrument should sit spectrally |
| `depth_position` | `DepthPosition` | Front-to-back placement |
| `processing_rationale` | `Vec<ProcessingRationale>` | Why each insert effect is present |

**MixRole**: `Lead`, `Supporting`, `Foundation`, `Texture`, `Rhythmic`, `Ambient`, `Effect`, `Dialogue`.

**FrequencyAllocation**:

| Field | Type | Description |
|-------|------|-------------|
| `fundamental_range` | `(f32, f32)` | Expected fundamental frequency range in Hz |
| `presence_range` | `(f32, f32)` | Frequency range where this instrument should be prominent |
| `avoid_range` | `Option<(f32, f32)>` | Frequency range to attenuate to avoid masking |

**DepthPosition**: `FrontClose`, `FrontMid`, `Mid`, `MidFar`, `Far`, `VeryFar`. Depth is created through a combination of reverb send level, pre-delay, high-frequency roll-off, and level.

**ProcessingRationale**: Attached to each effect in the insert chain:

| Field | Type | Description |
|-------|------|-------------|
| `effect_id` | `Id<MixEffect>` | Which effect this explains |
| `purpose` | `String` | e.g., "Remove low-frequency rumble below vocal range", "Tame harsh 3kHz resonance", "Control dynamic range for consistent level in the mix" |

---

## 3. Mix Effect Chain

### 3.1 MixEffectChain

**Definition 3.1.1**. A *MixEffectChain* is an ordered sequence of mix-stage effects. This is structurally identical to the Timbre IR's EffectChain but semantically distinct: Timbre IR effects shape the instrument's identity; Mix IR effects shape how it sits in the mix.

| Field | Type | Description |
|-------|------|-------------|
| `effects` | `Vec<MixEffect>` | Ordered processing chain |

### 3.2 MixEffect

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<MixEffect>` | Unique identifier |
| `effect_type` | `MixEffectType` | Processing algorithm |
| `enabled` | `bool` | Bypass state |
| `parameters` | `MixEffectParameters` | Type-specific parameters |
| `parameter_map` | `Map<String, MixDeviceParameter>` | Effect-relative scalar source path → explicit Live target mapping |

### 3.3 Mix-Stage EQ

**Definition 3.3.1**. Parametric equaliser for spectral shaping in the mix context.

| Field | Type | Description |
|-------|------|-------------|
| `bands` | `Vec<EQBand>` | Up to 8 bands |
| `linear_phase` | `bool` | Linear phase mode (higher latency, no phase distortion) |
| `auto_gain` | `bool` | Compensate for overall gain changes |

**EQBand** (same structure as Timbre IR §5.3.5):

| Field | Type | Description |
|-------|------|-------------|
| `frequency` | `f32` | Centre/corner frequency in Hz |
| `gain` | `f32` | Gain in dB |
| `q` | `f32` | Bandwidth (Q factor; higher = narrower) |
| `band_type` | `EQBandType` | `Peak`, `LowShelf`, `HighShelf`, `LowCut`, `HighCut`, `TiltShelf` |
| `dynamic` | `Option<DynamicEQConfig>` | Makes this band respond dynamically to signal level |

**DynamicEQConfig**: Turns a static EQ band into a dynamic one (the band engages only when the signal exceeds a threshold in that frequency region):

| Field | Type | Description |
|-------|------|-------------|
| `threshold` | `f32` | Level in dB at which the band begins to engage |
| `ratio` | `f32` | How aggressively the band responds |
| `attack` | `f32` | Response time in ms |
| `release` | `f32` | Recovery time in ms |

### 3.4 Mix-Stage Dynamics

#### 3.4.1 Compressor

| Field | Type | Description |
|-------|------|-------------|
| `threshold` | `f32` | Threshold in dB |
| `ratio` | `f32` | Compression ratio |
| `attack` | `f32` | Attack time in ms |
| `release` | `f32` | Release time in ms |
| `knee` | `f32` | Knee width in dB |
| `makeup_gain` | `f32` | Output gain in dB |
| `detection` | `DetectionMode` | `Peak`, `RMS`, `Envelope` |
| `topology` | `CompressorTopology` | `FeedForward`, `FeedBack` |
| `sidechain` | `SidechainConfig` | Internal or external sidechain |
| `stereo_link` | `f32` | 0.0 = independent L/R, 1.0 = fully linked |

**SidechainConfig**:

| Field | Type | Description |
|-------|------|-------------|
| `source` | `SidechainSource` | `Internal`, `External { channel_id: Id<ChannelStrip> }`, `ExternalBus { bus_id: Id<GroupBus> }` |
| `filter` | `Option<SidechainFilter>` | Filter applied to the sidechain signal |

**SidechainFilter**:

| Field | Type | Description |
|-------|------|-------------|
| `filter_type` | `EQBandType` | Typically `HighPass` or `BandPass` |
| `frequency` | `f32` | Frequency in Hz |
| `q` | `f32` | Bandwidth |

#### 3.4.2 Gate / Expander

| Field | Type | Description |
|-------|------|-------------|
| `threshold` | `f32` | Threshold in dB |
| `ratio` | `f32` | Expansion ratio (gate: ∞:1; expander: 2:1–10:1) |
| `attack` | `f32` | Attack in ms |
| `hold` | `f32` | Hold time in ms (minimum time gate stays open) |
| `release` | `f32` | Release in ms |
| `range` | `f32` | Maximum attenuation in dB (−∞ for hard gate) |
| `sidechain` | `SidechainConfig` | Detection source |

#### 3.4.3 Limiter

| Field | Type | Description |
|-------|------|-------------|
| `ceiling` | `f32` | Output ceiling in dBFS |
| `release` | `f32` | Release time in ms |
| `lookahead` | `f32` | Lookahead time in ms (trades latency for transparency) |
| `algorithm` | `LimiterAlgorithm` | `Brickwall`, `TruePeak`, `ISP` (inter-sample peak) |

#### 3.4.4 Multiband Dynamics

| Field | Type | Description |
|-------|------|-------------|
| `crossover_frequencies` | `Vec<f32>` | Band split points in Hz |
| `bands` | `Vec<MultibandDynamicsBand>` | Per-band compressor/expander settings |
| `crossover_slope` | `CrossoverSlope` | `Pole2` (12 dB/oct), `Pole4` (24 dB/oct), `LinearPhase` |

**MultibandDynamicsBand**:

| Field | Type | Description |
|-------|------|-------------|
| `compressor` | `Option<CompressorSettings>` | Downward compression for this band |
| `expander` | `Option<ExpanderSettings>` | Upward/downward expansion |
| `gain` | `f32` | Post-dynamics gain for this band in dB |
| `solo` | `bool` | Solo this band (for monitoring) |

### 3.5 Saturation and Harmonic Processing

| Field | Type | Description |
|-------|------|-------------|
| `algorithm` | `SaturationType` | Processing model |
| `drive` | `f32` | Input drive amount (0.0–1.0) |
| `mix` | `f32` | Dry/wet (parallel saturation) |
| `output_level` | `f32` | Gain compensation in dB |

**SaturationType**: `Tape { speed: TapeSpeed, bias: f32 }`, `Tube { model: String }`, `Transformer`, `Console { type: ConsoleType }`, `Soft`, `Hard`.

**TapeSpeed**: `Ips15` (15 inches/second — brighter), `Ips30` (30 ips — cleaner).

**ConsoleType**: `Neve`, `SSL`, `API`, `Generic`.

### 3.6 Stereo Processing

| Field | Type | Description |
|-------|------|-------------|
| `width` | `f32` | Stereo width (0.0 = mono, 1.0 = original, >1.0 = widened) |
| `mid_side_balance` | `f32` | Mid/side balance (0.0 = mid only, 0.5 = equal, 1.0 = side only) |
| `mono_below` | `Option<f32>` | Collapse frequencies below this Hz to mono |

### 3.7 Delay and Reverberation

Mix-stage spatial effects are distinct members of `MixEffectParameters`; an aux bus never uses an
EQ or dynamics processor as a stand-in for a requested delay or reverb.

**MixDelay**:

| Field | Type | Constraint |
|-------|------|------------|
| `tempo_synced` | `bool` | Selects exact beat division or free milliseconds |
| `delay_ms` | `f32` | Active when free-running; `(0, 10000]` ms |
| `beat_division` | `Beat` | Active when tempo-synchronised; positive exact rational |
| `feedback` | `f32` | `[0, 1]` |
| `stereo_mode` | `MixDelayMode` | `Mono`, `Stereo`, `PingPong` |
| `stereo_offset` | `f32` | `[0, 1000]`, in ms or beat offset according to mode |
| `low_cut_hz`, `high_cut_hz` | `f32` | `20 <= low < high <= 20000` Hz |
| `modulation_rate` | `f32` | `[0, 20]` Hz |
| `modulation_depth` | `f32` | `[0, 100]` ms |
| `mix` | `f32` | Dry/wet `[0, 1]` |

**MixReverb**:

| Field | Type | Constraint |
|-------|------|------------|
| `algorithm` | `MixReverbAlgorithm` | `Algorithmic`, `Convolution`, `Plate`, `Spring`, `Chamber`, `Hall`, `Room`, `Shimmer` |
| `impulse_response` | `String` | Required and non-empty for `Convolution` |
| `shimmer_pitch` | `f32` | Active for `Shimmer`; `[-24, 24]` semitones |
| `decay_time` | `f32` | RT60 in `(0, 120]` seconds |
| `pre_delay` | `f32` | `[0, 500]` ms |
| `damping`, `diffusion`, `size` | `f32` | Each in `[0, 1]` |
| `early_reflections_level` | `f32` | `[0, 1]` |
| `low_cut_hz`, `high_cut_hz` | `f32` | `20 <= low < high <= 20000` Hz |
| `mix` | `f32` | Dry/wet `[0, 1]`; normally `1` on an aux return |

### 3.8 Explicit target parameter mapping

**Definition 3.8.1**. A `MixDeviceParameter` declares a conversion from one materialised numeric,
boolean, or enum-valued field of its owning `MixEffect` into one public Live
`DeviceParameter`. It does not infer target names or ranges from the native-device type.

| Field | Type | Constraint |
|-------|------|------------|
| `parameter_name` | `String` | Non-empty exact public `name` or `original_name` |
| `source_min`, `source_max` | `f32` | Finite, `source_min < source_max` |
| `range_min`, `range_max` | `f32` | Finite target interval, `range_min < range_max` |
| `curve` | `MappingCurve` | `Linear`, `Exponential`, `Logarithmic`, `SCurve`, `ReverseSCurve`, or validated piecewise-linear `Custom` |
| `value_property` | `DeviceParameterValueProperty` | Internal `value` or GUI-visible `display_value` |

For current source value `x`, define `u = (x-source_min)/(source_max-source_min)`. Validation
requires `x` inside the declared source interval. The deployed value is

`y = range_min + (range_max-range_min) C(u)`,

where `C` is the selected deterministic curve from the Timbre mapping algebra. A custom curve has
finite points ordered from `(0,0)` to `(1,1)` and uses linear interpolation between adjacent
points. Mapping two source paths in the same effect to the same target name is an error.

The scalar path inventory is total over the active semantic fields of the materialised effect value: vector indices use
`bands[i]`/`crossover_frequencies[i]`, nested fields use dots, booleans map to `0` or `1`, and enums
map to their schema ordinal. Optional fields exist only when engaged; discriminated fields exist
only for the selected algorithm/mode (for example synced `beat_division` versus free `delay_ms`,
and `shimmer_pitch` only for Shimmer). External
sidechain IDs, saturation tube-model strings, and convolution impulse-response identifiers are
reported separately as non-`DeviceParameter` residual paths; scalar coverage never implies that
those routing/resource obligations were deployed.

---

## 4. Group Bus

### 4.1 GroupBus

**Definition 4.1.1**. A *GroupBus* sums multiple channels and applies collective processing.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<GroupBus>` | Unique identifier |
| `name` | `String` | Display name (e.g., "Strings", "Brass", "Drums", "All Synths") |
| `member_channels` | `Vec<Id<ChannelStrip>>` | Channels feeding this bus |
| `member_groups` | `Vec<Id<GroupBus>>` | Child group buses (for nesting) |
| `insert_chain` | `MixEffectChain` | Bus-level processing |
| `fader` | `Fader` | Bus level |
| `spatial` | `SpatialPosition` | Bus-level spatial positioning |
| `mute` | `bool` | Bus mute |
| `solo` | `bool` | Bus solo |
| `sends` | `Vec<AuxSendLevel>` | Bus-level aux sends |
| `output` | `GroupOutput` | Where this bus routes to |
| `intent` | `Option<GroupIntent>` | Semantic mixing intent for the group |

**GroupOutput**: `Master` (directly to master bus) or `Group { id: Id<GroupBus> }` (to a parent group).

**GroupIntent**:

| Field | Type | Description |
|-------|------|-------------|
| `function` | `String` | e.g., "Glue the string section together", "Parallel compression for drum punch" |
| `internal_balance_description` | `String` | e.g., "Violin I slightly prominent, violas and cellos supporting" |

### 4.2 Common Group Configurations

The following are standard group bus configurations for orchestral and production contexts.

**Orchestral**:

```
Master
 ├── Woodwinds (Flutes, Oboes, Clarinets, Bassoons)
 ├── Brass (Horns, Trumpets, Trombones, Tuba)
 ├── Percussion (Timpani, Cymbals, etc.)
 ├── Strings
 │    ├── High Strings (Violin I, Violin II, Viola)
 │    └── Low Strings (Cello, Double Bass)
 ├── Harp
 └── Keyboard (if present)
```

**Production / Electronic**:

```
Master
 ├── Drums
 │    ├── Kick
 │    ├── Snare
 │    ├── Hi-Hat / Cymbals
 │    └── Percussion
 ├── Bass
 ├── Harmonic (Pads, Keys, Guitars)
 ├── Lead (Vocal, Synth Lead)
 └── FX (Risers, Impacts, Textures)
```

These are templates; the agent configures the group structure to suit the production.

---

## 5. Auxiliary Bus (Send/Return)

### 5.1 AuxBus

**Definition 5.1.1**. An *AuxBus* provides parallel effects processing shared across multiple channels.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<AuxBus>` | Unique identifier |
| `name` | `String` | Display name (e.g., "Hall Reverb", "Plate Reverb", "Delay Throw", "Chorus Bus") |
| `effect_chain` | `MixEffectChain` | Processing on the aux bus (typically 100% wet) |
| `return_level` | `f32` | Return level in dB |
| `return_spatial` | `SpatialPosition` | Spatial position of the return |
| `output` | `AuxOutput` | Where the return routes to |
| `intent` | `Option<String>` | e.g., "Shared concert hall space for all orchestral instruments" |

**AuxOutput**: `Master`, `Group { id: Id<GroupBus> }`.

### 5.2 Common Auxiliary Bus Patterns

| Pattern | Purpose | Typical Effect |
|---------|---------|---------------|
| Shared Reverb | Create a common acoustic space | Convolution or algorithmic reverb, 100% wet |
| Depth Layers | Different reverb for front/mid/back | Multiple reverbs with different pre-delays and decay times |
| Delay Throw | Rhythmic echo on specific moments | Tempo-synced delay, automated send level |
| Parallel Compression | Add density without losing transients | Heavy compression, blended under the dry signal |
| Chorus/Width | Stereo enhancement | Chorus or short stereo delay |

### 5.3 Spatial Cohesion Through Shared Reverb

For orchestral and acoustic music, a critical mixing principle is that instruments sharing the same physical space should share the same reverb. The Mix IR enforces this through the aux bus structure: all instruments intended to sound as though they are in the same room send to the same reverb bus, with send levels varying to control perceived depth.

The agent uses the `DepthPosition` from the ChannelIntent (§2.4) to determine send levels:

| Depth | Reverb Send Level | Direct/Reverb Ratio |
|-------|-------------------|-------------------|
| FrontClose | −18 to −12 dB | Mostly direct |
| FrontMid | −12 to −6 dB | More direct than reverb |
| Mid | −6 to 0 dB | Balanced |
| MidFar | 0 to +3 dB | More reverb than direct |
| Far | +3 to +6 dB | Mostly reverb |
| VeryFar | +6 dB or higher | Almost entirely reverb |

These are heuristic guidelines; the specific values depend on the reverb's character and the desired spatial impression.

---

## 6. Spatial Positioning

### 6.1 Coordinate System

**Definition 6.1.1**. Spatial positioning in the Mix IR uses a three-dimensional perceptual coordinate system.

| Axis | Dimension | Range | Description |
|------|-----------|-------|-------------|
| `pan` | Left–Right | −1.0 to +1.0 | −1.0 = hard left, 0.0 = centre, +1.0 = hard right |
| `depth` | Near–Far | 0.0 to 1.0 | 0.0 = closest, 1.0 = most distant |
| `elevation` | Low–High | −1.0 to +1.0 | For immersive formats; 0.0 = ear level |

The normative rendering intent maps stereo `depth` through reverb send level, pre-delay, HF roll-off, and level, and maps all three axes for surround/immersive targets. The current Ableton compiler applies `pan` only; `depth`, `elevation`, `width`, and immersive placement remain IR data and are not rendered.

### 6.2 SpatialPosition

**Definition 6.2.1**.

| Field | Type | Description |
|-------|------|-------------|
| `pan` | `f32` | Left-right position (−1.0 to +1.0) |
| `depth` | `f32` | Near-far position (0.0 to 1.0) |
| `elevation` | `f32` | Vertical position (−1.0 to +1.0); 0.0 for stereo |
| `width` | `f32` | Source width (0.0 = point source, 1.0 = full stereo width) |
| `pan_law` | `PanLaw` | Level compensation for panning |
| `spatial_mode` | `SpatialMode` | Rendering method |

**PanLaw**: `ConstantPower` (−3 dB at centre), `Linear` (−6 dB at centre), `ConstantPowerSinCos`, `Custom { center_attenuation: f32 }`.

**SpatialMode**: `SimplePan` (pan pot only), `BalancePan` (stereo balance, preserving width), `BinauralHRTF` (head-related transfer function for headphone rendering), `ObjectBased { object_id: u32 }` (Atmos/immersive object).

### 6.3 Spatial Automation

Spatial positions can be automated over score time for moving sources (e.g., a melody that moves from left to right across the sound stage, or a source that approaches from the distance).

This is an IR capability. The current Ableton compiler preserves these lanes and reports the public-LOM automation capability gap rather than writing envelopes.

| Field | Type | Description |
|-------|------|-------------|
| `channel_id` | `Id<ChannelStrip>` | Which channel moves |
| `breakpoints` | `Vec<SpatialBreakpoint>` | Time-position pairs |
| `interpolation` | `InterpolationMode` | How position is interpolated |

**SpatialBreakpoint**:

| Field | Type | Description |
|-------|------|-------------|
| `time` | `ScoreTime` | Position in the score |
| `position` | `SpatialPosition` | Spatial position at this time |

### 6.4 Orchestral Seating Presets

Standard orchestral seating arrangements as spatial position templates:

**American seating** (Stokowski):

| Section | Pan | Depth |
|---------|-----|-------|
| Violin I | −0.6 to −0.2 | 0.2–0.4 |
| Violin II | −0.2 to +0.2 | 0.2–0.4 |
| Viola | +0.2 to +0.5 | 0.3–0.5 |
| Cello | +0.5 to +0.7 | 0.3–0.5 |
| Double Bass | +0.7 to +0.9 | 0.4–0.6 |
| Flutes | −0.3 to −0.1 | 0.4–0.5 |
| Oboes | −0.1 to +0.1 | 0.4–0.5 |
| Clarinets | +0.1 to +0.3 | 0.4–0.5 |
| Bassoons | +0.3 to +0.5 | 0.4–0.6 |
| Horns | −0.5 to −0.2 | 0.5–0.7 |
| Trumpets | −0.2 to +0.2 | 0.6–0.7 |
| Trombones | +0.2 to +0.5 | 0.6–0.7 |
| Tuba | +0.4 | 0.6–0.7 |
| Timpani | +0.3 to +0.5 | 0.7–0.8 |
| Percussion | −0.5 to +0.5 | 0.7–0.8 |
| Harp | −0.7 | 0.3–0.5 |

**European seating** (traditional, with second violins on the right):

Differs primarily in that Violin II is placed at +0.2 to +0.6 (right side) and cellos move to the left, creating antiphonal string effects.

These are starting-point templates; the agent adjusts positions based on the specific production context.

---

## 7. Master Bus

### 7.1 MasterBus

**Definition 7.1.1**. The *MasterBus* is the final processing stage before output.

| Field | Type | Description |
|-------|------|-------------|
| `insert_chain` | `MixEffectChain` | Master processing chain |
| `fader` | `Fader` | Master output level |
| `output_format` | `OutputFormat` | Stereo, surround, etc. |
| `dithering` | `Option<DitheringConfig>` | Dithering for bit-depth reduction |
| `target_loudness` | `Option<LoudnessTarget>` | Target integrated loudness |
| `metering` | `MeteringConfig` | What meters to display |

### 7.2 Master Processing Chain

A typical master bus chain, ordered by signal flow:

| Stage | Purpose | Typical Processing |
|-------|---------|-------------------|
| 1. Gain staging | Optimal level into processing | Trim |
| 2. Surgical EQ | Remove problematic resonances | Narrow-Q cuts |
| 3. Bus compression | Glue and cohesion | Gentle compression (2:1–4:1, slow attack) |
| 4. Tonal EQ | Broad spectral shaping | Broad shelves and wide peaks |
| 5. Stereo processing | Width and mono compatibility | Mid/side EQ, stereo width |
| 6. Saturation | Harmonic warmth | Tape or console emulation |
| 7. Limiting | Peak control and loudness | True-peak limiter |
| 8. Metering | Monitoring | LUFS, true peak, correlation |

Each stage is optional; the chain is configured by the agent based on genre and style requirements.

### 7.3 LoudnessTarget

**Definition 7.3.1**. Target loudness parameters for the master output.

| Field | Type | Description |
|-------|------|-------------|
| `integrated_lufs` | `f32` | Target integrated loudness in LUFS |
| `true_peak_dbfs` | `f32` | Maximum true-peak level in dBFS |
| `loudness_range_lu` | `Option<f32>` | Target loudness range in LU (dynamic range) |
| `standard` | `LoudnessStandard` | Target delivery standard |

**LoudnessStandard**:

| Standard | Integrated LUFS | True Peak dBFS | Context |
|----------|----------------|----------------|---------|
| `StreamingLoud` | −14 | −1.0 | Spotify, Apple Music (normalised) |
| `StreamingDynamic` | −16 | −1.0 | Classical, jazz (streaming, preserving dynamics) |
| `Broadcast` | −23 | −1.0 | EBU R128 broadcast |
| `Film` | −24 | −2.0 | Film/TV dialogue normalisation |
| `Vinyl` | −12 to −9 | −0.5 | Vinyl mastering (higher loudness, limited by medium) |
| `Custom { lufs: f32, peak: f32 }` | configurable | configurable | User-defined |

### 7.4 DitheringConfig

| Field | Type | Description |
|-------|------|-------------|
| `target_bit_depth` | `u8` | Output bit depth (16, 24) |
| `algorithm` | `DitherAlgorithm` | `TPDF` (triangular probability density), `NoiseShaping { order: u8 }`, `Pow-r { level: u8 }` |
| `auto_blank` | `bool` | Disable dither during silence |

### 7.5 MeteringConfig

| Field | Type | Description |
|-------|------|-------------|
| `lufs` | `bool` | Show LUFS meter (short-term, momentary, integrated) |
| `true_peak` | `bool` | Show true-peak meter |
| `rms` | `bool` | Show RMS level |
| `correlation` | `bool` | Show stereo correlation / phase meter |
| `spectrum` | `bool` | Show real-time spectrum analyser |
| `dynamics` | `bool` | Show gain reduction meter per dynamics processor |

---

## 8. Reference Profile System

### 8.1 Purpose

The *ReferenceProfile* system allows an agent to mix toward the characteristics of a reference recording. Mixing by reference is standard professional practice: engineers compare their work against commercially released mixes to calibrate their spectral balance, dynamic range, loudness, and spatial characteristics. The Mix IR formalises this as a queryable target.

### 8.2 ReferenceProfile

**Definition 8.2.1**. A *ReferenceProfile* captures the analysed characteristics of a reference mix.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `Id<ReferenceProfile>` | Unique identifier |
| `name` | `String` | Reference name (e.g., "Mahler Symphony No. 2 — Bernstein/VPO", "Daft Punk — Get Lucky") |
| `source` | `Option<String>` | Audio file path or streaming reference |
| `spectral_profile` | `SpectralProfile` | Average spectral balance |
| `dynamic_profile` | `DynamicProfile` | Dynamic range characteristics |
| `loudness_profile` | `LoudnessProfile` | Loudness measurements |
| `spatial_profile` | `SpatialProfile` | Stereo width and correlation |
| `tonal_balance_curve` | `Vec<(f32, f32)>` | Frequency → dB target curve |

### 8.3 SpectralProfile

| Field | Type | Description |
|-------|------|-------------|
| `average_spectrum` | `Vec<(f32, f32)>` | (frequency Hz, magnitude dB) — long-term average |
| `spectral_centroid` | `f32` | Average spectral centroid in Hz |
| `spectral_tilt` | `f32` | Slope of the average spectrum (dB/octave) |
| `band_energies` | `Vec<BandEnergy>` | Energy in ISO frequency bands |

**BandEnergy**: Energy measurements in standard frequency bands (sub-bass 20–60 Hz, bass 60–250 Hz, low-mid 250–500 Hz, mid 500–2 kHz, upper-mid 2–4 kHz, presence 4–6 kHz, brilliance 6–20 kHz).

### 8.4 DynamicProfile

| Field | Type | Description |
|-------|------|-------------|
| `crest_factor` | `f32` | Peak-to-RMS ratio in dB |
| `loudness_range` | `f32` | LRA in LU (per ITU-R BS.1770) |
| `dynamic_range_dr` | `f32` | DR value (per Pleasurize Music Foundation) |
| `peak_to_loudness_ratio` | `f32` | PLR in dB |

### 8.5 LoudnessProfile

| Field | Type | Description |
|-------|------|-------------|
| `integrated` | `f32` | Integrated LUFS |
| `short_term_max` | `f32` | Maximum short-term LUFS (3-second window) |
| `momentary_max` | `f32` | Maximum momentary LUFS (400 ms window) |
| `true_peak` | `f32` | Maximum true-peak level in dBFS |

### 8.6 SpatialProfile

| Field | Type | Description |
|-------|------|-------------|
| `average_correlation` | `f32` | Average stereo correlation (−1.0 to +1.0; +1.0 = mono) |
| `average_width` | `f32` | Perceived stereo width (0.0–1.0) |
| `mid_side_ratio` | `f32` | Average mid/side energy ratio |
| `low_frequency_mono_coherence` | `f32` | Correlation below 200 Hz (should be high for speaker compatibility) |

### 8.7 Reference Comparison Query

An agent can query the current mix state against a reference profile:

```
compare_to_reference(reference_id) → ReferenceComparison
```

**ReferenceComparison**:

| Field | Type | Description |
|-------|------|-------------|
| `spectral_deviation` | `Vec<(f32, f32)>` | (frequency, dB difference) — where the mix deviates from the reference spectrum |
| `loudness_difference` | `f32` | LUFS difference |
| `dynamic_range_difference` | `f32` | LRA/DR difference |
| `width_difference` | `f32` | Stereo width difference |
| `recommendations` | `Vec<MixRecommendation>` | Suggested adjustments |

**MixRecommendation**:

| Field | Type | Description |
|-------|------|-------------|
| `target` | `String` | What to adjust (e.g., "Master EQ", "Strings group level", "Reverb decay time") |
| `suggestion` | `String` | e.g., "Reduce 200–400 Hz by approximately 2 dB to match reference clarity" |
| `confidence` | `f32` | How closely this maps to the reference difference (0.0–1.0) |

---

## 9. Mix Automation

### 9.1 MixAutomation

**Definition 9.1.1**. *MixAutomation* provides time-varying control of any Mix IR parameter, synchronised to the score timeline.

| Field | Type | Description |
|-------|------|-------------|
| `target` | `MixAutomationTarget` | What parameter is being automated |
| `breakpoints` | `Vec<AutomationBreakpoint>` | Time-value pairs |
| `interpolation` | `InterpolationMode` | `Step`, `Linear`, `Smooth`, `Exponential` |
| `intent` | `Option<String>` | Why this automation exists |

**AutomationBreakpoint**:

| Field | Type | Description |
|-------|------|-------------|
| `time` | `ScoreTime` | Position in the score |
| `value` | `f32` | Parameter value at this time |

### 9.2 MixAutomationTarget

Targets are addressed by their structural path in the MixGraph:

Examples:
- `channels[part_id].fader.level_db` — channel fader level
- `channels[part_id].spatial.pan` — channel pan position
- `channels[part_id].sends[aux_id].level_db` — aux send level
- `channels[part_id].insert_chain.effects[0].parameters.threshold` — compressor threshold
- `group_buses[group_id].fader.level_db` — group bus level
- `master_bus.insert_chain.effects[2].parameters.ratio` — master compressor ratio
- `aux_sends[aux_id].effect_chain.effects[0].parameters.decay_time` — reverb decay

### 9.3 Common Automation Patterns

| Pattern | Description | Implementation |
|---------|-------------|---------------|
| Vocal ride | Maintain consistent vocal level across dynamic phrases | Automate channel fader to compensate for level variations |
| Build/drop | Gradual increase in energy followed by sudden change | Automate filter cutoffs, reverb sends, group levels |
| Reverb throw | Momentary increase in reverb for effect | Automate aux send level: brief peak on specific notes/words |
| Width automation | Narrow mix for verse, widen for chorus | Automate stereo width on master or group buses |
| Spectral shift | Darken verse, brighten chorus | Automate EQ shelf gain on master or group |
| Sidechain pumping | Rhythmic ducking driven by kick drum | Automate compressor threshold or use sidechain source |
| Spatial movement | Moving instrument across the sound stage | Automate pan and depth over score time |

---

## 10. Compilation

### 10.1 Compilation to Ableton

The *MixCompiler* maps the Mix IR to the supported portion of Ableton's mixer and effects infrastructure. Its result distinguishes accepted calls, immediate scalar readback equality, and semantic completeness, and includes warnings for requested operations outside the public LOM surface.

The production transport first obtains the versioned target profile defined in the
bridge contract (`remote_script/Sunny/bridge_contract.json`); its absence is a pre-mutation protocol failure, including for an
offline recording transport that has not declared a modelled target. When AuxBuses are present, it also requires the existing
return-track count before mutation so new AuxBus IDs resolve to the returns actually appended to a
non-empty set. Missing count evidence is a protocol failure; a graph with no AuxBus neither needs
nor reads that target fact.

**Mapping**:

| Mix IR Element | Ableton Element |
|---------------|-----------------|
| ChannelStrip | Track (MIDI or Audio) mixer section |
| ChannelStrip.fader | `Track.mixer_device.volume.display_value` |
| ChannelStrip.spatial.pan | `Track.mixer_device.panning.value` |
| ChannelStrip.mute / solo | Track Mute / Solo plus exact Track Activator `mute ? 0.0 : 1.0` |
| ChannelStrip.insert_chain | Native devices inserted on the track |
| GroupBus | Capability warning; public LOM cannot create Group Tracks |
| GroupBus.insert_chain | Preserved in IR; not emitted without a Group Track |
| AuxBus | Return Track |
| AuxBus.effect_chain | Native devices inserted on the return track |
| AuxBus.return_level | `ReturnTrack.mixer_device.volume.display_value` |
| AuxBus.return_spatial.pan | `ReturnTrack.mixer_device.panning.value` in Stereo Pan mode |
| AuxBus implicit gate | Return mute/solo false; crossfade assignment none; Track Activator 1.0 |
| AuxSendLevel.level_db | `Track.mixer_device.sends[index].display_value` |
| AuxSendLevel.pre_fader | Explicit unsupported pre/post-mode obligation |
| MasterBus | Master Track |
| MasterBus.insert_chain | Native devices inserted on the master track |
| MasterBus.fader | `MasterTrack.mixer_device.volume.display_value` |
| MasterBus implicit neutral controls | Main Track Activator 1.0; Stereo Pan mode; pan 0.0 |
| MixAutomation | Capability warning; public LOM cannot author envelopes |

**Compilation steps**:

1. **Preflight and fader algebra**: Validate the graph and solve the complete relative-fader
   dependency system before the first target mutation. Invalid references/cycles fail. Preserve
   exact static solutions and measurement-dependent residual states in the result. Every derived
   channel, return, send, and device path must fit the bridge's canonical non-negative signed-index
   domain; reject an unrepresentable range before mutation. Summary counters are 64-bit and do not
   inherit this addressing limit.

2. **Track structure**: Use the channel tracks created by the Score compiler. If AuxBuses are
   requested, inspect the current return count and fail before mutation when it is unavailable;
   otherwise create one return track per AuxBus starting at that exact count. GroupBus requests are
   reported, not replaced with audio tracks. Each generated Return receives an explicit false
   mute/solo gate, crossfade assignment 1 (neither A nor B), panning mode 0 (Stereo), Track
   Activator 1.0, and a retained
   AuxBus-ID/index deployment record; no host default supplies these obligations.

3. **Channel processing**: In aggregate project compilation, resolve each ChannelStrip's `part_id`
   through the Score-derived Part-to-track map, independently of ChannelStrip collection order,
   then insert enabled effects in IR order. The legacy standalone compiler accepts an explicit
   `base_track` and therefore uses channel collection order by contract. Device insertion requires
   Live 12.3+. Whenever at least one enabled effect is materialisable, inspect the track's current
   device count before insertion. If the count is `n`, the first inserted effect is appended and
   verified as `devices[n]`, the next as `devices[n+1]`, and so on; an unavailable count is a
   protocol failure rather than a guessed index. Each real response must prove the exact count/index
   delta, requested display class, audio-effect type, false `can_have_chains`, activity,
   non-negative reported latency in samples/milliseconds, and audio-output/non-MIDI-output Track
   classification. A Rack is not a faithful lowering of one flat `MixEffect`: its parallel/nested
   chains and chain-owned selection/mixer state have no source carrier.

4. **Routing**: Resolve each AuxSendLevel by `aux_bus_id` to its preflighted return-track index and
   set the corresponding send-level DeviceParameter. Count the independent pre/post-mode request
   but do not infer it from a Track default: the current admitted surface writes zero modes and
   therefore zero complete logical sends. An unknown aux reference violates validation rule
   X6 and rejects the complete graph before mutation; it is not a partial-deployment warning. The
   compiler also inventories every Channel, Group, and Aux output edge as an exact source→target
   residual. The public Track API exposes writable output-routing dictionaries, but their
   identifiers are target-owned and `Song.create_midi_track`/`create_return_track` document no
   Master-routing postcondition. With no explicit binding, Sunny writes and verifies zero such
   edges rather than trusting a default. A caller may admit an exact named-target type/channel pair
   plus provenance for a materialisable Channel/Aux-to-Master edge; the compiler then performs the
   current-protocol two-stage membership/readback transaction. Group and unbound edges remain
   residuals, and verification remains conditional on the admitted mapping.

5. **Levels and spatial**: Set each statically resolved channel/master value; for
   measurement-dependent entries set the explicit fallback and retain an exact residual. Then set
   channel pan positions and mute/solo states, lowering each Channel mute to exact floating Track
   Activator 0.0/1.0. Set every Aux return level, modeled pan, and Activator 1.0. Because MasterBus
   has no mute or pan intent while Live's Main Track owns both controls, set the faithful neutral
   projection: Main Activator 1.0, panning mode 0, and pan 0.0. Retain depth, elevation, width,
   non-default pan law, and non-simple spatial mode as explicit warnings.

6. **Master chain**: Insert the master bus processing chain on the master track.

7. **Effect parameters**: Resolve each declared effect-relative source path, apply Definition 3.8.1,
   select exactly one enabled target parameter by exact `name`/`original_name`, validate internal
   target ranges where applicable, write the mapped value, and retain the requested range and
   action plus resolved identity, observed internal bounds, quantisation, the conditional
   continuous-default or opaque quantized-label domain, enabled,
   active/automation state, and immediate readback evidence. Equal readback with inactive state or
   non-zero automation state remains incomplete. A real effect that fails structural verification
   cannot anchor a parameter address: retain the resolved requested deployment without issuing the
   dependent write. Recording plans may continue through modeled unexecuted structure. Report total scalar paths, explicit mappings, exact missing
   paths, non-parameter residual paths, deployed mappings, and verified mappings separately.

8. **Readback**: For return names, faders, pan, activators, mute, solo, sends, Main pan mode, and
   master level, require the
   current-protocol bridge to return the property, echoed request, and immediate observed value. Retain
   one `property_deployments` record per write and report a divergent observation.

9. **Automation**: Preserve MixAutomation in the IR and report it as unsupported by the current public-LOM target. Return `automation_lanes_requested = |automation|` and `automation_lanes_written = 0` as distinct 64-bit evidence counters, so absence of a request cannot be confused with failure to deploy one.

The result applies the same cardinality rule to structural projections: requested/created pairs for
Group and Return Tracks, requested/inserted/verified for every enabled effect across channel, group, aux, and
master chains, requested/configured for enabled sends and channel strips, and
`output_routes_requested`/`output_routes_written`/`output_routes_verified` for output edges. Exact
unwritten edges appear in `output_route_residuals`. An effect retained on
an unmaterialisable GroupBus therefore remains requested even though it cannot enter the inserted
count. `device_deployments` retains every attempted materialised effect in deterministic traversal
order, including bounded public Device latency reports. Those reports are not summed and the result
keeps complete render-path latency false because compensation/monitoring mode, Track Delay,
routing, buffers/drivers, external hardware, and acoustic propagation remain outside the model;
successful-call counters remain distinct from verified target state.

**Live capability boundary**:

- `Track.insert_device` requires Live 12.3+ and supports native Live devices only; plug-ins and Max
  for Live devices are not inserted. An observed older target skips effects before mutation while
  applying the remaining representable mixer state.
- A real effect insertion with missing/malformed evidence is a protocol failure. Well-formed wrong
  identity/type, Rack shape, inactivity, or loss of audio-output classification is retained and
  makes the result incomplete. Dependent parameter writes are skipped for that effect. Recording
  plans claim zero effect verification but retain their modeled parameter commands. Sunny does not
  guess Rack chains, zones, selectors, chain mixers, pads, return chains, or nested devices.
- Public LOM cannot create or assign Group Tracks, configure all sidechain relationships, or author automation envelopes.
- Public Track `output_routing_type` and `output_routing_channel` are get/set dictionaries whose
  values must come from the target's available-routing collections. The current protocol and snapshot schema retains
  both exact selected dictionaries and the exact one-key available type/channel wrappers on every
  normal and Return Track. With no binding, every output edge remains an explicit residual. A
  caller may conditionally bind a materialisable Channel or Aux-to-Master edge to one exact
  type/channel pair returned by a named target, but must retain non-empty mapping provenance;
  Sunny does not infer semantic identity from either target-owned string. Group destinations are
  never bindable because no corresponding Track is materialised.
- A bound output route is two separate journalled mutations. The type stage requires current
  advertised membership before setting and exact membership/readback afterward. Because changing
  type may replace the channel collection, the channel stage revalidates the selected type and
  both current collections before setting, then requires stable type, exact channel readback, and
  final membership. Both peers independently close every nested evidence shape. A recording plan
  reports `recorded_only` and verifies nothing; only complete real evidence removes that edge's
  residual. Project postconditions require the exact final pair and the retained admitted mapping.
  This verification is conditional on provenance, not an independent proof that an opaque symbol
  means Main. Plan equality includes the complete binding and provenance. Partial mutation,
  named-Live private representation, future changes, Main hardware output, signal, and sound remain
  explicit external obligations; `has_audio_output` remains signal-type classification rather than
  destination evidence.
- Normal Track input routing is retained as a separate target-state boundary for generated Parts.
  The current protocol and snapshot schema requires exact audio/MIDI-input Booleans and, when input-capable, selected
  type/channel dictionaries that occur in the corresponding exact available-option arrays. The
  Mix IR has no input-source carrier, so membership is not promoted to source identity or external
  input neutrality and no input-routing write is admitted.
- The current protocol and snapshot schema also retains exact `[0,1]` floating input/output one-second hold peaks on
  audio/MIDI normal Tracks. It retains exact left/right momentary input/output meter floats only
  when the normal Track reports audio output, with explicit nulls otherwise. Generated Parts
  require both hold peaks and all four channel values to be exactly zero, under separate hold,
  momentary-stereo, and conjunctive verdicts. The adapter reads each documented meter once because
  Live warns that the channel meters add GUI load. Continuous input/output silence stays false:
  this is bounded sequential activity evidence, not the Mix IR's loudness/true-peak model, a
  signal-path trace, or rendered-audio validation.
- Input trim, polarity, non-pan spatial fields, pre/post-fader send mode, non-master aux output,
  non-stereo output, dithering, and automatic loudness targeting are preserved but not applied; any
  non-default request is reported. Channel/Group relative levels are solved statically. A
  `MasterTarget` relation remains a measured-audio residual and uses its explicit fallback.
- Only explicitly mapped scalar effect fields are applied. Partial coverage, target-ineligible group
  chains, external sidechain routing IDs, tube-model strings, and convolution resource loading
  remain explicit residuals; no device-default or approximate parameter-name guess is permitted.
- Missing or malformed property evidence from a real transport is a protocol failure. A recording
  transport emits the same deterministic property plan with null observations and no verification
  claim.
- Snapshot schema 34 records finite internal bounds, exact Boolean quantisation/enablement, the
  conditional finite in-range continuous default or exact opaque quantized-label vector, active
  state, and automation state for every selected mixer parameter. Final scalar evidence
  requires the requested value, the operation's documented domain class, enabled true,
  `state = 0`, and `automation_state = 0`: Track Activators are quantised; volume, panning, and
  sends are unquantised. Internal-value requests must also lie inside the observed bounds;
  display-value requests retain those internal bounds without comparing incompatible domains.
  Separate Part/Return/Main gates impose the same rule on Track Activators, and the Main gate
  additionally requires an unquantised centered Stereo Pan. Equal numbers on a wrong-domain,
  disabled, inactive, or automated control are not effective-state proofs.
- Unsupported or observably divergent requests make `complete` false. Scalar readback equality does
  not prove effect parameterisation, signal routing outside the mapped fields, or audible equivalence.

### 10.2 Device Mapping

Each MixEffect type maps to a deterministic native Ableton device name:

| MixEffect Type | Ableton Device |
|---------------|----------------|
| EQ (parametric) | EQ Eight |
| Compressor | Compressor |
| Gate | Gate |
| Limiter | Limiter |
| Multiband Dynamics | Multiband Dynamics |
| Saturation | Saturator |
| Stereo Width | Utility |
| Delay | Delay |
| Reverb | Reverb |

The effect-to-device-name mapping is fixed by the current compiler. Parameter names, domains, and
curves are not fixed: they are declared per effect through `parameter_map`, because Live exposes
them as device/version facts. The exact inserted device address is compiler-derived, not
caller-supplied.

---

## 11. Validation

### 11.1 Structural Validation

| Rule | Severity | Description |
|------|----------|-------------|
| X0 | Error | Channel, group, aux, reference, and MixEffect identities are unique; effect IDs are unique across the entire graph |
| X1 | Error | Every Score IR Part has a corresponding ChannelStrip |
| X2 | Error | Signal flow graph is acyclic |
| X3 | Error | Every channel reaches the master bus through some path |
| X4 | Warning | Channel has no insert processing (intentional?) |
| X5 | Warning | Channel fader is at −∞ (silence) but not muted |
| X6 | Error | Sidechain source references a non-existent channel or bus |
| X7 | Error | Any effect variant violates its documented finite numeric/cardinality domain |
| X8 | Error | An effect target mapping has an unresolved path, invalid domain/curve, empty target, or same-effect target alias |
| X9 | Error | Relative faders contain a missing reference, dependency cycle, non-finite value, invalid loudness target, or derived value above +12 dB |
| X10 | Error | Channel/group assignments or child/parent group routing do not exactly mirror the corresponding member list, including dangling or duplicate members |
| X11 | Error | A Channel/Group send level is non-finite or the same source contains more than one send record for an AuxBus |

X7 uses a deliberately target-independent, tractable source envelope. Device-specific intervals
remain mapping facts:

| Family | Source-domain envelope |
|--------|------------------------|
| EQ | at most 8 bands; frequency `[20,20000]` Hz; gain `[-120,120]` dB; Q `(0,100]`; dynamic threshold `[-160,60]`, ratio `[1,1000]`, attack `[0,60000]` ms, release `(0,60000]` ms |
| Compressor | threshold `[-160,60]` dB; ratio `[1,1000]`; attack `[0,60000]` ms; release `(0,60000]` ms; knee `[0,60]` dB; makeup `[-120,120]` dB; stereo link `[0,1]` |
| Gate | compressor time/ratio envelopes; hold `[0,60000]` ms; attenuation range `[-160,0]` dB |
| Sidechain filter | frequency `[20,20000]` Hz; Q `(0,100]` |
| Limiter | ceiling `[-60,6]` dBFS; release `(0,60000]` ms; lookahead `[0,1000]` ms |
| Multiband | at most 7 strictly increasing crossovers in `[20,20000]`; band count equals crossover count plus one when configured; nested dynamics obey their own envelopes; band gain `[-120,120]` dB |
| Saturation | drive/mix `[0,1]`; output `[-120,120]` dB; Tape bias `[-1,1]`; Tube requires a non-empty model identifier |
| Stereo | width `[0,4]`; mid/side balance `[0,1]`; optional mono-below `[20,20000]` Hz |
| Delay/Reverb | exact envelopes in §3.7 |

### 11.2 Audio Quality Validation [E]

| Rule | Severity | Description |
|------|----------|-------------|
| A1 | Error | Master output exceeds 0 dBFS true peak |
| A2 | Warning | Master output exceeds loudness target by more than 1 LU |
| A3 | Warning | Stereo correlation below 0.0 for extended periods (phase cancellation risk) |
| A4 | Warning | Sub-bass (< 80 Hz) stereo correlation below 0.5 (mono compatibility issue) |
| A5 | Info | Master output loudness range exceeds reference by more than 3 LU |
| A6 | Warning | Channel output clips before reaching the group bus or master |
| A7 | Info | Spectral deviation from reference profile exceeds 3 dB in any band |

### 11.3 Mix Intent Validation

| Rule | Severity | Description |
|------|----------|-------------|
| I1 | Info | Channel has no ChannelIntent annotation |
| I2 | Info | Group has no GroupIntent annotation |
| I3 | Warning | Channel marked as `Lead` role but fader level is below average |
| I4 | Warning | Channel marked as `Foundation` but has a high-pass filter removing sub-bass |
| I5 | Info | Depth positions are all the same (flat depth staging) |

---

## 12. Agent Interface

### 12.1 MCP Tools

**Configuration tools**:

| Tool | Description |
|------|-------------|
| `create_mix_graph` | Initialise the Mix IR from the Score IR's part list |
| `create_group_bus` | Reject missing/duplicate members before mutation, create a group bus, and move each requested channel to one exact mirrored membership edge |
| `create_aux_bus` | Create an auxiliary bus with effects |
| `assign_channel_to_group` | Remove every stale/duplicate reverse edge and route the channel to exactly one group bus |
| `assign_group_to_group` | Transactionally route one GroupBus into another with one exact mirror edge, rejecting cycles/depth violations |
| `route_group_to_master` | Transactionally remove every parent mirror edge and route the GroupBus directly to Master |
| `set_channel_send` | Set a channel's send level to an aux bus |
| `apply_seating_template` | Apply an orchestral seating preset to spatial positions |
| `set_output_format` | Set stereo, surround, or immersive output |

**Processing tools**:

| Tool | Description |
|------|-------------|
| `add_channel_effect` | Add an EQ, compressor, or other effect to a channel |
| `add_bus_effect` | Add processing to a group bus |
| `add_aux_effect` | Add processing to an aux bus |
| `add_master_effect` | Add processing to the master bus chain |
| `map_mix_effect_parameter` | Declare and preflight an effect-relative source-to-DeviceParameter mapping |
| `set_channel_level` | Set an absolute fader value and clear prior relative intent |
| `set_channel_relative_level` | Set and transactionally preflight a channel relation |
| `resolve_mix_fader_levels` | Return the complete static solution and measured residuals without contacting Ableton |
| `set_channel_pan` | Set spatial position |
| `set_channel_depth` | Set depth position |
| `set_loudness_target` | Set the master loudness target |

**Automation tools**:

| Tool | Description |
|------|-------------|
| `add_mix_automation` | Add parameter automation |

**Analysis and reference tools**:

| Tool | Description |
|------|-------------|
| `create_reference_profile` | Store caller-supplied measurements for a reference recording |
| `compare_to_reference` | Compare current mix to a reference |

**Intent tools**:

| Tool | Description |
|------|-------------|
| `set_channel_intent` | Set the mixing intent for a channel |
| `set_group_intent` | Set the mixing intent for a group |

**Compilation**:

| Tool | Description |
|------|-------------|
| `compile_mix` | Compile supported mixer operations and report completeness |
| `validate_mix` | Run all validation rules |
| `get_mix_json` | Serialise the current Mix IR graph |

### 12.2 Agent Workflow: Mixing an Orchestral Work

1. `create_mix_graph` — initialise from Score IR parts.
2. `create_group_bus` — create Woodwinds, Brass, Percussion, Strings groups.
3. `apply_seating_template("American")` — set initial spatial positions.
4. `create_aux_bus("Concert Hall")` — shared reverb for orchestral space.
5. For each channel: `set_channel_send` to the concert hall reverb, level based on depth position.
6. For each channel: `set_channel_intent` describing role and frequency allocation.
7. `set_channel_level` for initial balance (relative levels: "Violin I at reference, Violin II 2 dB below", etc.).
8. For each channel: `add_channel_effect` — EQ to carve frequency space, light compression for dynamic control.
9. For each group: `add_bus_effect` — gentle bus compression for section cohesion.
10. Master chain: `add_master_effect` — surgical EQ, bus compression, limiter.
11. `set_loudness_target` — e.g., StreamingDynamic at −16 LUFS for classical.
12. `create_reference_profile` — store measurements obtained from a reference recording.
13. `compare_to_reference` — identify spectral and dynamic deviations.
14. Iterate: adjust EQ, levels, reverb sends based on comparison.
15. `validate_mix` — check for clipping, phase issues, intent consistency.
16. `compile_mix` — push to Ableton.

### 12.3 Agent Workflow: Mixing an Electronic Track

1. `create_mix_graph`.
2. `create_group_bus` — Drums, Bass, Harmonic, Lead, FX.
3. `create_aux_bus("Room Verb")`, `create_aux_bus("Plate Verb")`, `create_aux_bus("Delay")`.
4. Set spatial positions: kick and bass centre; hi-hats slightly off-centre; pads wide; lead centre.
5. `add_channel_effect(effect_type="compressor", sidechain_source=...)` — bass ducking from kick.
6. `add_channel_effect(effect_type="eq", bands=...)` — high-pass on everything except kick and bass; presence boost on lead.
7. `add_bus_effect` on Drums — parallel compression for punch.
8. `add_mix_automation` — filter sweep on pads through the build; reverb throw on vocal in the pre-chorus.
9. Master chain: saturation → EQ → compression → limiter.
10. `set_loudness_target(StreamingLoud, -14 LUFS)`.
11. `create_reference_profile` with caller-supplied measurements from a reference track.
12. Iterate toward reference.
13. `compile_mix`.

---

## 13. Serialisation

### 13.1 On-Disk Format

The implemented Mix IR format is schema version 3 JSON (same conventions as the Score IR:
snake_case fields, Beat values as `{"n": ..., "d": ...}`, enumerations as integers according to
the current schema). Version 2 added `delay` and `reverb`; version 3 requires every encoded effect
to carry a `parameter_map` object. The reader migrates version 1 and 2 effects to an empty map,
without inventing target facts. Unknown effect discriminators or malformed mappings are format
errors, and full X0–X8 validation runs after parsing. No binary Mix IR codec is implemented or
advertised.

### 13.2 Invariant Re-Validation on Load

Full validation (§11) runs on deserialisation before the document is made available.

---

## 14. Cross-Specification Integration

### 14.1 The Four-Layer Stack

The complete Sunny production system consists of four formally specified layers:

| Layer | Specification | Domain | Source of Truth For |
|-------|--------------|--------|-------------------|
| 1. Theory | Theory Spec | Musical knowledge | Pitch, intervals, chords, scales, voice leading, form, transformations |
| 2. Score | Score IR Spec | Musical content | What is played: notes, rhythms, dynamics, articulations, structure |
| 3. Timbre | Timbre IR Spec | Sound design | How each instrument sounds: synthesis, sampling, per-instrument effects |
| 4. Mix | Mix IR Spec | Production | How instruments combine: levels, EQ, compression, spatial positioning, mastering |

**Dependency direction**: Each layer depends on the layers above it and is independent of the layers below it.

- The Theory Spec depends on nothing.
- The Score IR depends on the Theory Spec (imports types).
- The Timbre IR depends on the Score IR (one TimbreProfile per Part).
- The Mix IR depends on the Score IR (one ChannelStrip per Part) and the Timbre IR (the channel input is the Timbre IR output).

**Compilation order**: The aggregate project compiler validates exact Part correspondence, derives
the Score-authoritative Part-to-track map, then compiles Score, Timbre, and Mix in that order into
the same Ableton session. See `sunny-project-model.md`.

### 14.2 The Score IR's DynamicBalance and the Mix IR's Fader

The Score IR's OrchestrationLayer includes a `DynamicBalance` annotation (Foreground, MiddleGround, Background). The Mix IR's ChannelStrip includes a `Fader` with absolute and relative levels. These interact as follows:

- The Score IR compiler uses DynamicBalance to adjust MIDI velocities (§9.5 of the Score IR Spec).
- A Mix authoring policy may use DynamicBalance as an *input signal* for choosing initial fader and
  reverb-send levels (Foreground → higher fader / less reverb, Background → lower fader / more
  reverb). The current project compiler does not derive or overwrite MixGraph values from this
  annotation; explicit Mix IR state remains authoritative and independently inspectable.
- The two systems are complementary, not redundant: the Score IR's velocity adjustment affects the *performance intensity* of the instrument, while the Mix IR's level adjustment affects the *mix balance*. A fortissimo violin in the background (high velocity, low fader) sounds different from a piano violin in the foreground (low velocity, high fader), even at the same perceived loudness.

### 14.3 Unified Agent Interface

The MCP server exposes tools from all four layers in a unified namespace. An agent composing and producing a piece uses tools from all layers in a single workflow, moving fluidly between composition ("write a melody"), sound design ("set a warm pad sound"), and mixing ("send this to the hall reverb at −6 dB").

The deployed MCP server exposes the following registration groups:

| Registration group | Tool count | Examples |
|--------------------|-----------:|----------|
| Core and Ableton | 10 | `analyze_harmony`, `create_progression_clip`, `get_ableton_session_state` |
| Score IR | 29 | `score_create`, `score_insert_chord_symbol`, `score_compile_to_ableton` |
| Timbre IR | 23 | `set_sound_source`, `map_timbre_parameter`, `compile_timbre` |
| Mix IR | 28 | `set_channel_relative_level`, `resolve_mix_fader_levels`, `compile_mix` |
| Corpus IR | 22 | `ingest_midi`, `remove_ingested_work`, `query_style_profile` |
| Project | 4 | `project_validate`, `project_plan_to_ableton`, `project_apply_ableton_plan`, `project_compile_to_ableton` |

Total: 116 tools. `tools/list` is the runtime authority.

---

## 15. Invariants

### 15.1 Structural

1. Every Score IR Part has exactly one ChannelStrip.
2. The signal flow graph is a DAG (no cycles).
3. Every channel reaches the master bus.
4. Group bus nesting does not exceed a configurable maximum depth (default: 4).
5. Aux send levels are finite (no +∞ dB sends).
6. The relative-fader dependency graph is referentially closed and acyclic; only
   measurement-dependent loudness targets may remain unresolved.
7. Channel/group assignments and parent/child group membership lists are exact bidirectional
   mirrors with no duplicate or dangling member.
8. Each Channel or Group has at most one finite send level per AuxBus; every send target exists.

### 15.2 Empirical Audio-Quality Targets

The following require rendered-audio measurement and are not enforced by the current parameter-only runtime:

6. Master bus output should not exceed the configured true-peak ceiling.
7. No individual channel or bus output should exceed 0 dBFS before gain staging is applied.

### 15.3 Compilation

8. The MixCompiler produces one deterministic supported command stream per Mix IR and reports every unrepresentable requested feature.
9. All automation breakpoints reference valid ScoreTime positions.
10. Sidechain sources reference existing channels or buses.

---

*End of specification.*
