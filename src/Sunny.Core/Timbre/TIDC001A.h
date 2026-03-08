/**
 * @file TIDC001A.h
 * @brief Timbre IR document model — TimbreProfile, SoundSource, EffectChain
 *
 * Component: TIDC001A
 * Domain: TI (Timbre IR) | Category: DC (Document)
 *
 * Defines the complete document model for the Timbre IR: the TimbreProfile
 * root, SoundSource sum type (8 synthesis paradigms), EffectChain, and all
 * supporting composite types.
 *
 * Composes with Timbre IR foundation types (TITP001A) and Score IR types
 * (PartId, ScoreTime).
 *
 * Invariants:
 * - Every Score IR Part has exactly one TimbreProfile
 * - Signal flow within a TimbreProfile is acyclic (guaranteed by Vec structure)
 * - Modulation targets reference valid parameter paths
 * - Timbral automation breakpoints reference valid ScoreTime positions
 */

#pragma once

#include "TITP001A.h"

#include <memory>
#include <variant>

namespace Sunny::Core {

// =============================================================================
// Sound Source Variants (§2.2–§2.8)
// =============================================================================

/**
 * @brief Classic analogue synthesis: oscillators → mixer → filter → amplifier
 */
struct SubtractiveSynth {
    std::vector<Oscillator> oscillators;
    std::optional<NoiseGenerator> noise;
    std::vector<float> oscillator_mix;
    Filter filter;
    std::optional<Filter> filter_2;
    enum class FilterRouting : std::uint8_t { Serial, Parallel };
    FilterRouting filter_routing = FilterRouting::Serial;
    AmplifierEnvelope amplifier;
    std::optional<UnisonConfig> unison;
    std::optional<PortamentoConfig> portamento;
};

/**
 * @brief Frequency modulation synthesis with operator network
 */
struct FMSynth {
    std::vector<FMOperator> operators;
    FMAlgorithm algorithm;
    float feedback = 0.0f;
};

/**
 * @brief Wavetable crossfade synthesis
 */
struct WavetableSynth {
    WavetableRef wavetable;
    float position = 0.0f;                      // 0.0–1.0
    std::uint32_t frame_count = 256;
    enum class Interpolation : std::uint8_t { Linear, Cubic, Spectral };
    Interpolation interpolation = Interpolation::Linear;
    std::optional<Filter> filter;
    AmplifierEnvelope amplifier;
};

/**
 * @brief Granular synthesis from an audio source buffer
 */
struct GranularSynth {
    AudioBufferRef source;
    float grain_size = 50.0f;                   // ms (1–500)
    float grain_density = 20.0f;                // grains per second
    float position = 0.0f;                      // 0.0–1.0
    float position_random = 0.0f;
    float pitch_random = 0.0f;                  // cents
    GrainEnvelope grain_envelope;
    float spray = 0.0f;
    float stereo_spread = 0.0f;
    float reverse_probability = 0.0f;
};

/**
 * @brief Additive synthesis with individual partial control
 */
struct AdditiveSynth {
    std::uint16_t partial_count = 16;
    std::vector<PartialDefinition> partials;
    AmplifierEnvelope global_envelope;
};

/**
 * @brief Physical modelling synthesis (waveguide/modal)
 */
struct PhysicalModelSource {
    PhysicalModelConfig model;
    ExciterConfig exciter;
    ResonatorConfig resonator;
    float coupling = 0.5f;
    float damping = 0.5f;
    float brightness = 0.5f;
};

/**
 * @brief Sample playback with multi-layer mapping
 */
struct SamplerSource {
    std::string library;
    std::string preset;
    SampleMap sample_map;
    std::vector<MicrophonePosition> microphone_positions;
    bool release_samples = false;
    RoundRobinMode round_robin_mode = RoundRobinMode::RoundRobin;
    std::optional<AmplifierEnvelope> envelope_override;
    std::optional<Filter> filter;
    float tuning_offset = 0.0f;                 // cents
};

// =============================================================================
// SoundSource — recursive sum type via unique_ptr indirection (§2.1, §2.9)
// =============================================================================

struct SoundSourceData;

/**
 * @brief Layer routing determines how multiple SoundSource layers combine.
 *
 * The type field selects the active routing strategy; fields for other
 * strategies carry their defaults and are ignored.
 */
struct LayerRouting {
    LayerRoutingType type = LayerRoutingType::Mix;
    std::vector<float> mix_levels;
    std::vector<std::pair<std::uint8_t, std::uint8_t>> velocity_ranges;
    std::vector<std::pair<SpelledPitch, SpelledPitch>> key_ranges;
    ModulationSource crossfade_source;
    MappingCurve crossfade_curve;
};

/**
 * @brief Hybrid source combining multiple SoundSource layers
 *
 * Uses unique_ptr to SoundSourceData for recursion. Moving a HybridSource
 * transfers ownership of the child layers.
 */
struct HybridSource {
    std::vector<std::unique_ptr<SoundSourceData>> layers;
    LayerRouting routing;
};

using SoundSourceVariant = std::variant<
    SubtractiveSynth,
    FMSynth,
    WavetableSynth,
    GranularSynth,
    AdditiveSynth,
    PhysicalModelSource,
    SamplerSource,
    HybridSource
>;

/**
 * @brief Concrete SoundSource node wrapping the variant.
 *
 * The indirection through unique_ptr<SoundSourceData> in HybridSource
 * resolves the recursive type: SoundSourceData contains HybridSource
 * (by value in the variant), and HybridSource contains
 * unique_ptr<SoundSourceData> (which needs only forward declaration).
 */
struct SoundSourceData {
    SoundSourceVariant data;
};

// =============================================================================
// Effect Chain (§5.1–§5.2)
// =============================================================================

using EffectParameters = std::variant<
    DistortionEffect,
    DelayEffect,
    ReverbEffect,
    ChorusEffect,
    PhaserEffect,
    FlangerEffect,
    EQEffect,
    CompressorEffect
>;

/**
 * @brief Single signal processor in the insert chain
 */
struct Effect {
    EffectId id;
    EffectParameters parameters;
    bool enabled = true;
    float mix = 1.0f;              // dry/wet blend: 0.0 = dry, 1.0 = wet
};

/**
 * @brief Ordered sequence of effects; signal flows left to right
 */
struct EffectChain {
    std::vector<Effect> effects;
    bool bypass_all = false;
};

// =============================================================================
// Preset Morphing (§7.2)
// =============================================================================

struct PresetMorph {
    TimbrePresetId from_preset;
    TimbrePresetId to_preset;
    ScoreTime start;
    ScoreTime end;
    MappingCurve curve;
};

// =============================================================================
// Timbre Preset (§8.1)
// =============================================================================

struct TimbrePreset {
    TimbrePresetId id;
    std::string name;
    std::optional<std::string> description;
    SemanticTimbreDescriptor semantic_descriptors;
    std::map<std::string, float> parameter_state;
    std::vector<std::string> tags;
};

// =============================================================================
// Rendering Configuration (§9.1)
// =============================================================================

struct TimbreRenderingConfig {
    DeviceType device_type;
    std::map<std::string, DeviceParameter> parameter_map;
    std::optional<std::string> preset_path;
};

// =============================================================================
// TimbreProfile — root node (§1.1)
// =============================================================================

/**
 * @brief Root object of a single instrument's timbral configuration.
 *
 * Each Part in the Score IR has exactly one associated TimbreProfile.
 * The signal flow is: SoundSource → InsertChain → [Mix IR channel].
 * The ModulationMatrix operates at the profile level, connecting
 * modulation sources (LFOs, envelopes, MIDI CCs) to any numeric
 * parameter within the profile via path-based targeting.
 */
struct TimbreProfile {
    TimbreProfileId id;
    PartId part_id;
    std::string name;
    SoundSourceData source;
    ModulationMatrix modulation;
    EffectChain insert_chain;
    SemanticTimbreDescriptor semantic_descriptors;
    std::vector<TimbreAutomation> parameter_automation;
    std::vector<PresetMorph> preset_morphs;
    std::vector<TimbrePreset> presets;
    TimbreRenderingConfig rendering;
};

}  // namespace Sunny::Core
