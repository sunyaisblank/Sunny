/**
 * @file TITP001A.h
 * @brief Timbre IR foundation types — enumerations, value types, identifiers
 *
 * Component: TITP001A
 * Domain: TI (Timbre IR) | Category: TP (Types)
 *
 * Defines all enumerations, small value types, and identifier types required
 * by the Timbre IR document model (Timbre Spec §1–§9). These types compose
 * with Score IR types (PartId, ScoreTime, Beat) without duplicating them.
 *
 * Invariants:
 * - All normalised parameter values are in [0.0, 1.0] unless a physical
 *   unit (Hz, dB, ms) is specified
 * - Signal flow within a TimbreProfile is acyclic
 * - All Id<T> are unique within a document
 */

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include "../Score/SITP001A.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Identifiers
// =============================================================================

struct TimbreProfileTag;
struct EffectTag;
struct TimbrePresetTag;

using TimbreProfileId = Id<TimbreProfileTag>;
using EffectId        = Id<EffectTag>;
using TimbrePresetId  = Id<TimbrePresetTag>;

// =============================================================================
// Error Codes (6000–6099)
// =============================================================================

namespace TimbreError {
    constexpr int MissingProfile       = 6000;  // T1: Part without TimbreProfile
    constexpr int InvalidSource        = 6001;  // T2: Malformed SoundSource
    constexpr int CutoffAboveNyquist   = 6010;  // T3: Filter cutoff > Nyquist
    constexpr int ExcessiveDetune      = 6011;  // T4: Oscillator detune > ±100ct
    constexpr int FMFeedbackUnstable   = 6012;  // T5: FM feedback exceeds threshold
    constexpr int EffectChainCycle     = 6020;  // T6: Cycle in effect chain
    constexpr int InvalidModTarget     = 6021;  // T7: Bad modulation target path
    constexpr int InvalidParameter     = 6022;  // Parameter value rejected by validation
    constexpr int NotFound             = 6023;  // Effect, macro, or preset not found
    constexpr int DuplicateId          = 6024;  // Duplicate effect or macro index
    constexpr int StaleDescriptors     = 6030;  // T8: Descriptors out of date
    constexpr int UnmappedParameters   = 6031;  // T9: Rendering config gaps
}  // namespace TimbreError

// =============================================================================
// Opaque references to external audio data
// =============================================================================

using WavetableRef  = std::string;
using AudioBufferRef = std::string;
using ModulationTarget = std::string;

// =============================================================================
// §2.2.1 Waveform
// =============================================================================

enum class WaveformType : std::uint8_t {
    Sine, Triangle, Saw, Square, Pulse, SuperSaw, Custom
};

struct HarmonicPartial {
    std::uint16_t partial_number;
    float amplitude;
    float phase;
};

struct Waveform {
    WaveformType type = WaveformType::Sine;
    float super_saw_detune = 0.0f;
    std::uint8_t super_saw_voices = 7;
    std::vector<HarmonicPartial> custom_harmonics;
};

// =============================================================================
// §2.2.2 Noise
// =============================================================================

enum class NoiseColour : std::uint8_t { White, Pink, Brown, Blue, Violet };

struct NoiseGenerator {
    NoiseColour colour = NoiseColour::White;
    float level = 0.0f;
};

// =============================================================================
// §2.2.3 Filter
// =============================================================================

enum class FilterSlope : std::uint8_t {
    Pole1,  // 6 dB/oct
    Pole2,  // 12 dB/oct
    Pole3,  // 18 dB/oct
    Pole4   // 24 dB/oct
};

enum class FilterCategory : std::uint8_t {
    LowPass, HighPass, BandPass, Notch, Comb,
    Formant, LadderLP, StateVariable, Diode
};

enum class FormantVowel : std::uint8_t { A, E, I, O, U };

struct FilterConfig {
    FilterCategory category = FilterCategory::LowPass;
    FilterSlope slope = FilterSlope::Pole2;
    float bandwidth = 1.0f;         // BandPass, Notch
    float comb_feedback = 0.5f;     // Comb
    FormantVowel vowel = FormantVowel::A;
};

// =============================================================================
// §2.2.4 Unison
// =============================================================================

struct UnisonConfig {
    std::uint8_t voice_count = 1;
    float detune = 0.0f;            // cents spread
    float stereo_spread = 0.0f;
    float blend = 0.5f;
};

// =============================================================================
// §2.2.5 Portamento
// =============================================================================

enum class PortamentoMode : std::uint8_t { Always, Legato };
enum class GlideCurve : std::uint8_t { Linear, Exponential, SCurve };

struct PortamentoConfig {
    float time = 100.0f;            // ms
    PortamentoMode mode = PortamentoMode::Legato;
    GlideCurve curve = GlideCurve::Linear;
};

// =============================================================================
// §2.2.1 Oscillator
// =============================================================================

struct Oscillator {
    Waveform waveform;
    std::int8_t tune_semitones = 0;
    float tune_cents = 0.0f;        // −100 to +100
    float phase = 0.0f;             // 0.0–1.0
    float pulse_width = 0.5f;       // 0.0–1.0; 0.5 = square
    float level = 1.0f;             // 0.0–1.0
};

// =============================================================================
// §3 Envelope
// =============================================================================

enum class EnvelopeCurve : std::uint8_t {
    Linear, Exponential, Logarithmic, SCurve, Step
};

struct EnvelopeStage {
    float duration = 0.0f;          // ms (0 = instantaneous)
    float target_level = 0.0f;      // 0.0–1.0
    EnvelopeCurve curve = EnvelopeCurve::Linear;
    float curvature = 1.0f;         // for Exponential/Logarithmic
};

struct EnvelopeLoop {
    std::uint8_t start_stage = 0;
    std::uint8_t end_stage = 0;
    std::uint8_t count = 0;         // 0 = infinite until note-off
};

struct Envelope {
    std::vector<EnvelopeStage> stages;
    std::optional<EnvelopeLoop> loop;
    float velocity_sensitivity = 0.0f;
    float key_tracking = 0.0f;
};

/// AmplifierEnvelope has identical structure to Envelope; the additional
/// constraint (final stage target must be 0.0) is enforced by validation.
using AmplifierEnvelope = Envelope;

// =============================================================================
// §2.2.3 Filter (full — depends on Envelope)
// =============================================================================

struct Filter {
    FilterConfig config;
    float cutoff = 1000.0f;         // Hz
    float resonance = 0.0f;        // 0.0–1.0 (1.0 = self-oscillation threshold)
    float drive = 0.0f;            // 0.0–1.0
    float key_tracking = 0.0f;     // 0.0 = none, 1.0 = full
    std::optional<Envelope> envelope;
    float envelope_depth = 0.0f;   // −1.0 to 1.0
};

// =============================================================================
// §2.3 FM Synthesis
// =============================================================================

struct FMRouting {
    std::uint8_t modulator;
    std::uint8_t carrier;
    float depth;
};

struct FMAlgorithm {
    bool use_preset = true;
    std::uint8_t preset_number = 1;             // 1–32 DX7-style
    std::vector<FMRouting> custom_routing;       // when !use_preset; must be DAG
};

struct FMOperator {
    float ratio = 1.0f;
    std::optional<float> fixed_frequency;       // Hz; overrides ratio
    float level = 1.0f;
    Envelope envelope;
    float detune = 0.0f;                        // cents
    Waveform waveform;
};

// =============================================================================
// §2.5 Granular Synthesis
// =============================================================================

enum class GrainEnvelopeShape : std::uint8_t {
    Gaussian, Hann, Triangle, Trapezoid, Rectangular
};

struct GrainEnvelope {
    GrainEnvelopeShape shape = GrainEnvelopeShape::Hann;
    float trapezoid_attack_ratio = 0.25f;
    float trapezoid_release_ratio = 0.25f;
};

// =============================================================================
// §2.6 Additive Synthesis
// =============================================================================

struct PartialDefinition {
    float ratio = 1.0f;                         // frequency ratio to fundamental
    float amplitude = 1.0f;
    float phase = 0.0f;
    std::optional<Envelope> envelope;
    float detune = 0.0f;                        // cents
};

// =============================================================================
// §2.7 Physical Model
// =============================================================================

enum class PhysicalModelCategory : std::uint8_t {
    String, Bar, Membrane, Tube, Plate, Reed, Lip, Bowed
};

enum class StringMaterial : std::uint8_t { Steel, Nylon, Gut, Wire };

struct PhysicalModelConfig {
    PhysicalModelCategory category = PhysicalModelCategory::String;
    StringMaterial string_material = StringMaterial::Steel;
    bool tube_open_end = true;
    float reed_stiffness = 0.5f;
    float lip_mass = 0.5f;
    float bow_pressure = 0.5f;
    float bow_speed = 0.5f;
};

enum class ExciterType : std::uint8_t {
    Impulse, Noise, Bow, Blow, Strike
};

struct ExciterConfig {
    ExciterType type = ExciterType::Strike;
    float amplitude = 0.8f;
    float brightness = 0.5f;
    float noise_mix = 0.1f;
};

struct ResonatorConfig {
    float frequency_ratio = 1.0f;
    float decay = 0.5f;
    float inharm = 0.0f;                        // inharmonicity coefficient
    std::uint8_t mode_count = 8;
};

// =============================================================================
// §2.8 Sampler
// =============================================================================

enum class RoundRobinMode : std::uint8_t { Sequential, Random, RoundRobin };

struct SampleZone {
    std::uint8_t low_key;
    std::uint8_t high_key;
    std::uint8_t low_velocity;
    std::uint8_t high_velocity;
    std::string articulation;
    std::string sample_path;
};

struct SampleMap {
    std::vector<SampleZone> zones;
};

struct MicrophonePosition {
    std::string name;
    float level = 1.0f;
    bool enabled = true;
};

// =============================================================================
// §2.9 Layer Routing
// =============================================================================

enum class LayerRoutingType : std::uint8_t {
    Mix, VelocitySplit, KeySplit, Crossfade
};

// =============================================================================
// §4.4 LFO
// =============================================================================

enum class LFOWaveformType : std::uint8_t {
    Sine, Triangle, Saw, Square, SampleAndHold, SmoothRandom, Custom
};

struct LFORate {
    bool synced = false;
    float hz = 1.0f;
    Beat division{1, 4};
};

struct LFO {
    LFOWaveformType waveform = LFOWaveformType::Sine;
    LFORate rate;
    float phase = 0.0f;
    float symmetry = 0.5f;
    bool retrigger = true;
    float fade_in = 0.0f;          // ms
    float delay = 0.0f;            // ms
    std::vector<std::pair<float, float>> custom_points;
};

// =============================================================================
// §4.5 Step Sequencer
// =============================================================================

enum class LoopMode : std::uint8_t { Forward, PingPong, OneShot };

struct StepSequencer {
    std::vector<float> steps;
    Beat step_duration{1, 16};
    float smooth = 0.0f;           // 0.0 = hard step, 1.0 = full interpolation
    LoopMode loop_mode = LoopMode::Forward;
};

// =============================================================================
// §4.6 Mapping Curve and Macro Knobs
// =============================================================================

enum class MappingCurveType : std::uint8_t {
    Linear, Exponential, Logarithmic, SCurve, ReverseSCurve, Custom
};

struct MappingCurve {
    MappingCurveType type = MappingCurveType::Linear;
    std::vector<std::pair<float, float>> custom_points;
};

struct MacroMapping {
    ModulationTarget target;
    float min = 0.0f;
    float max = 1.0f;
    MappingCurve curve;
};

struct MacroKnob {
    std::uint8_t index = 0;
    std::string name;
    float value = 0.0f;
    std::vector<MacroMapping> mappings;
};

// =============================================================================
// §4.2 Modulation Source and Routing
// =============================================================================

enum class ModulationSourceType : std::uint8_t {
    LFO, Envelope, Velocity, KeyPosition, Aftertouch, ModWheel,
    Expression, BreathController, PitchBend, CC, Random,
    StepSequencer, AudioFollower, MacroKnob
};

struct ModulationSource {
    ModulationSourceType type = ModulationSourceType::Velocity;
    std::uint8_t index = 0;         // LFO, Envelope, StepSequencer, MacroKnob
    std::uint8_t cc_number = 0;     // CC
    PartId sidechain_part{};        // AudioFollower
};

struct ModulationRouting {
    ModulationSource source;
    ModulationTarget target;
    float depth = 0.0f;            // −1.0 to 1.0 (bipolar)
    std::optional<ModulationSource> via;
};

// =============================================================================
// §4.1 Modulation Matrix (aggregates sources and routings)
// =============================================================================

struct ModulationMatrix {
    std::vector<ModulationRouting> routings;
    std::vector<LFO> lfos;
    std::vector<StepSequencer> step_sequencers;
    std::vector<MacroKnob> macro_knobs;
};

// =============================================================================
// §5.3.1 Distortion
// =============================================================================

enum class DistortionAlgorithmType : std::uint8_t {
    SoftClip, HardClip, FoldBack, TubeEmulation, TapeEmulation,
    Bitcrush, WaveShaper, RingModulation
};

struct DistortionAlgorithm {
    DistortionAlgorithmType type = DistortionAlgorithmType::SoftClip;
    std::uint8_t bit_depth = 8;
    float sample_rate_reduction = 1.0f;
    std::vector<std::pair<float, float>> waveshaper_curve;
    float ring_mod_frequency = 440.0f;          // Hz
};

struct DistortionEffect {
    DistortionAlgorithm algorithm;
    float drive = 0.0f;
    float tone = 0.5f;
    float output_level = 1.0f;
};

// =============================================================================
// §5.3.2 Delay
// =============================================================================

struct DelayTime {
    bool synced = false;
    float ms = 500.0f;
    Beat division{1, 4};
};

enum class StereoDelayMode : std::uint8_t { Mono, Stereo, PingPong };

struct DelayEffect {
    DelayTime delay_time;
    float feedback = 0.3f;
    StereoDelayMode stereo_mode = StereoDelayMode::Mono;
    float stereo_offset = 0.0f;     // ms or beat offset for Stereo mode
    std::optional<Filter> filter;   // feedback path filter
    float modulation_rate = 0.0f;   // Hz
    float modulation_depth = 0.0f;  // ms
};

// =============================================================================
// §5.3.3 Reverb
// =============================================================================

enum class ReverbAlgorithmType : std::uint8_t {
    Algorithmic, Convolution, Plate, Spring, Chamber, Hall, Room, Shimmer
};

struct ReverbAlgorithm {
    ReverbAlgorithmType type = ReverbAlgorithmType::Algorithmic;
    AudioBufferRef impulse_path;
    float shimmer_pitch = 12.0f;    // semitones
};

struct ReverbEffect {
    ReverbAlgorithm algorithm;
    float decay_time = 1.5f;        // RT60 in seconds
    float pre_delay = 20.0f;        // ms
    float damping = 0.5f;
    float diffusion = 0.7f;
    float size = 0.5f;
    float early_reflections_level = 0.5f;
    float eq_low_cut = 80.0f;       // Hz
    float eq_high_cut = 12000.0f;   // Hz
};

// =============================================================================
// §5.3.4 Modulation Effects
// =============================================================================

struct ChorusEffect {
    float rate = 1.0f;              // Hz
    float depth = 0.5f;
    std::uint8_t voices = 2;
    float feedback = 0.0f;
    float stereo_spread = 0.5f;
};

struct PhaserEffect {
    float rate = 0.5f;              // Hz
    float depth = 0.5f;
    std::uint8_t stages = 4;        // 2–24
    float feedback = 0.3f;
    float center_frequency = 1000.0f;   // Hz
};

struct FlangerEffect {
    float rate = 0.3f;              // Hz
    float depth = 0.5f;
    float feedback = 0.5f;          // negative for through-zero
    float manual = 5.0f;            // base delay time (ms)
};

// =============================================================================
// §5.3.5 EQ
// =============================================================================

enum class EQBandType : std::uint8_t {
    Peak, LowShelf, HighShelf, LowCut, HighCut
};

struct EQBand {
    float frequency = 1000.0f;      // Hz
    float gain = 0.0f;              // dB (−24 to +24)
    float q = 1.0f;
    EQBandType band_type = EQBandType::Peak;
};

struct EQEffect {
    std::vector<EQBand> bands;
};

// =============================================================================
// §5.3.6 Dynamics
// =============================================================================

struct SidechainConfig {
    PartId source;
    std::optional<Filter> filter;
};

struct CompressorEffect {
    float threshold = -20.0f;       // dB
    float ratio = 4.0f;
    float attack = 10.0f;           // ms
    float release = 100.0f;         // ms
    float knee = 0.0f;              // dB (0 = hard knee)
    float makeup_gain = 0.0f;       // dB
    std::optional<SidechainConfig> sidechain;
};

// =============================================================================
// §6 Semantic Timbral Descriptors
// =============================================================================

enum class AttackCharacter : std::uint8_t {
    Percussive, Plucked, Bowed, Blown, Gradual, Swelling, Explosive
};

enum class SustainCharacter : std::uint8_t {
    Steady, Decaying, Evolving, Pulsing, Swelling, Noisy
};

enum class DerivationMode : std::uint8_t {
    Manual, ParameterDerived, AudioDerived
};

struct SemanticTimbreDescriptor {
    float brightness = 0.5f;
    float warmth = 0.5f;
    float roughness = 0.0f;
    AttackCharacter attack_character = AttackCharacter::Percussive;
    SustainCharacter sustain_character = SustainCharacter::Steady;
    float width = 0.0f;
    float density = 0.5f;
    float movement = 0.0f;
    float weight = 0.5f;
    std::vector<std::string> tags;
    DerivationMode derivation = DerivationMode::Manual;
};

// =============================================================================
// §7 Timbral Automation
// =============================================================================

enum class AutomationInterpolation : std::uint8_t {
    Step, Linear, Smooth, Exponential
};

struct AutomationBreakpoint {
    ScoreTime time;
    float value;
};

struct TimbreAutomation {
    std::string parameter_path;
    std::vector<AutomationBreakpoint> breakpoints;
    AutomationInterpolation interpolation = AutomationInterpolation::Linear;
};

// =============================================================================
// §9 Rendering Configuration
// =============================================================================

enum class PluginFormat : std::uint8_t { VST, AU };

enum class DeviceTypeTag : std::uint8_t { NativeAbleton, Plugin };

struct DeviceType {
    DeviceTypeTag tag = DeviceTypeTag::NativeAbleton;
    std::string device_name;
    PluginFormat plugin_format = PluginFormat::VST;
    std::string plugin_identifier;
};

struct DeviceParameter {
    std::uint8_t device_index = 0;
    std::string parameter_name;
    float range_min = 0.0f;
    float range_max = 1.0f;
    MappingCurve curve;
};

}  // namespace Sunny::Core

#pragma GCC diagnostic pop
