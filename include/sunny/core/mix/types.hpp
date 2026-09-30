/**
 * @file types.hpp
 * @brief Mix IR foundation types — identifiers, enumerations, value types
 *
 *
 * Defines all identifiers, enumerations, and small value types required by
 * the Mix IR document model (Mix Spec §1–§9). These types compose with
 * Score IR types (PartId, ScoreTime) and reuse Timbre IR effect parameter
 * types (EQBand, CompressorEffect) where structurally identical.
 *
 * Invariants:
 * - Signal levels are finite (no +/-inf except Fader at -inf for silence)
 * - Spatial coordinates: pan in [-1,+1], depth in [0,1], elevation in [-1,+1]
 * - All Id<T> are unique within a MixGraph
 */

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <sunny/core/score/types.hpp>
#include <sunny/core/timbre/types.hpp>
#include <utility>
#include <vector>

namespace sunny::core {

// =============================================================================
// Identifiers (§1.2, §2.1, §3.2, §4.1, §5.1)
// =============================================================================

struct MixGraphTag;
struct ChannelStripTag;
struct GroupBusTag;
struct AuxBusTag;
struct MixEffectTag;
struct ReferenceProfileTag;

using MixGraphId = Id<MixGraphTag>;
using ChannelStripId = Id<ChannelStripTag>;
using GroupBusId = Id<GroupBusTag>;
using AuxBusId = Id<AuxBusTag>;
using MixEffectId = Id<MixEffectTag>;
using ReferenceProfileId = Id<ReferenceProfileTag>;

// =============================================================================
// §1.2 OutputFormat
// =============================================================================

enum class OutputFormat : std::uint8_t {
    Stereo,
    LCR,
    Quad,
    Surround51,
    Surround71,
    Atmos,
    Binaural
};

struct AtmosConfig {
    std::uint8_t bed_channels = 10;
    std::uint8_t object_count = 16;
};

// =============================================================================
// §2.2 Fader and RelativeLevel (§2.2.1)
// =============================================================================

enum class LevelReferenceType : std::uint8_t { MasterTarget, Channel, Group };

struct LevelReference {
    LevelReferenceType type = LevelReferenceType::MasterTarget;
    float lufs = -14.0f;         // For MasterTarget
    ChannelStripId channel_id{}; // For Channel
    std::string relationship;    // For Channel (e.g. "3 dB below Violin I")
    GroupBusId group_id{};       // For Group
};

struct RelativeLevel {
    LevelReference reference;
    float offset_db = 0.0f;
};

struct Fader {
    float level_db = 0.0f; // dB; -inf to +12
    std::optional<RelativeLevel> relative_level;
};

/** Identifies the fader whose effective level is being resolved. */
enum class FaderTargetType : std::uint8_t { Channel, Group, Master };

/**
 * Proof state for one fader-level resolution.
 *
 * Explicit and Resolved entries have a resolved_level_db. A
 * RequiresLoudnessMeasurement entry is a MasterTarget constraint that cannot
 * be solved without observed programme loudness. BlockedByUnresolvedReference
 * propagates that residual through channel/group dependencies.
 */
enum class FaderLevelResolutionStatus : std::uint8_t {
    Explicit,
    Resolved,
    RequiresLoudnessMeasurement,
    BlockedByUnresolvedReference
};

struct FaderLevelResolution {
    FaderTargetType target_type = FaderTargetType::Channel;
    std::uint64_t target_id = 0; // Master uses the distinguished value 0.
    float explicit_level_db = 0.0f;
    std::optional<float> resolved_level_db;
    FaderLevelResolutionStatus status = FaderLevelResolutionStatus::Explicit;
    std::optional<LevelReference> reference;
    float offset_db = 0.0f;
};

struct RelativeLevelResolution {
    std::vector<FaderLevelResolution> levels;
    std::uint32_t relative_levels_total = 0;
    std::uint32_t relative_levels_resolved = 0;
    std::uint32_t relative_levels_unresolved = 0;

    [[nodiscard]] bool complete() const noexcept { return relative_levels_unresolved == 0; }
};

// =============================================================================
// §2.3 AuxSendLevel
// =============================================================================

struct AuxSendLevel {
    AuxBusId aux_bus_id{};
    float level_db = -12.0f;
    bool pre_fader = false;
    bool enabled = true;
};

// =============================================================================
// §2.4 ChannelIntent
// =============================================================================

enum class MixRole : std::uint8_t {
    Lead,
    Supporting,
    Foundation,
    Texture,
    Rhythmic,
    Ambient,
    Effect,
    Dialogue
};

struct FrequencyAllocation {
    float fundamental_low = 0.0f;    // Hz
    float fundamental_high = 0.0f;   // Hz
    float presence_low = 0.0f;       // Hz
    float presence_high = 0.0f;      // Hz
    std::optional<float> avoid_low;  // Hz
    std::optional<float> avoid_high; // Hz
};

enum class DepthPosition : std::uint8_t { FrontClose, FrontMid, Mid, MidFar, Far, VeryFar };

struct ProcessingRationale {
    MixEffectId effect_id{};
    std::string purpose;
};

struct ChannelIntent {
    MixRole role_in_mix = MixRole::Supporting;
    FrequencyAllocation frequency_space;
    DepthPosition depth_position = DepthPosition::Mid;
    std::vector<ProcessingRationale> processing_rationale;
};

// =============================================================================
// §3.3 Mix-Stage EQ (extends Timbre IR EQBand with dynamic capability)
// =============================================================================

struct DynamicEQConfig {
    float threshold = -20.0f; // dB
    float ratio = 2.0f;
    float attack = 10.0f;   // ms
    float release = 100.0f; // ms
};

enum class MixEQBandType : std::uint8_t { Peak, LowShelf, HighShelf, LowCut, HighCut, TiltShelf };

struct MixEQBand {
    float frequency = 1000.0f; // Hz
    float gain = 0.0f;         // dB
    float q = 1.0f;
    MixEQBandType band_type = MixEQBandType::Peak;
    std::optional<DynamicEQConfig> dynamic;
};

struct MixEQ {
    std::vector<MixEQBand> bands; // Up to 8 bands
    bool linear_phase = false;
    bool auto_gain = false;
};

// =============================================================================
// §3.4 Mix-Stage Dynamics
// =============================================================================

enum class DetectionMode : std::uint8_t { Peak, RMS, Envelope };

enum class CompressorTopology : std::uint8_t { FeedForward, FeedBack };

enum class SidechainSourceType : std::uint8_t { Internal, ExternalChannel, ExternalBus };

struct SidechainFilter {
    MixEQBandType filter_type = MixEQBandType::HighCut;
    float frequency = 1000.0f; // Hz
    float q = 1.0f;
};

struct MixSidechainConfig {
    SidechainSourceType source = SidechainSourceType::Internal;
    ChannelStripId channel_id{}; // For ExternalChannel
    GroupBusId bus_id{};         // For ExternalBus
    std::optional<SidechainFilter> filter;
};

struct MixCompressor {
    float threshold = -20.0f; // dB
    float ratio = 4.0f;
    float attack = 10.0f;     // ms
    float release = 100.0f;   // ms
    float knee = 0.0f;        // dB (0 = hard knee)
    float makeup_gain = 0.0f; // dB
    DetectionMode detection = DetectionMode::RMS;
    CompressorTopology topology = CompressorTopology::FeedForward;
    MixSidechainConfig sidechain;
    float stereo_link = 1.0f; // 0.0 = independent, 1.0 = linked
};

struct MixGate {
    float threshold = -40.0f; // dB
    float ratio = 10.0f;      // Expansion ratio
    float attack = 0.5f;      // ms
    float hold = 50.0f;       // ms
    float release = 100.0f;   // ms
    float range = -80.0f;     // dB (max attenuation)
    MixSidechainConfig sidechain;
};

enum class LimiterAlgorithm : std::uint8_t { Brickwall, TruePeak, ISP };

struct MixLimiter {
    float ceiling = -1.0f;  // dBFS
    float release = 100.0f; // ms
    float lookahead = 5.0f; // ms
    LimiterAlgorithm algorithm = LimiterAlgorithm::TruePeak;
};

enum class CrossoverSlope : std::uint8_t {
    Pole2, // 12 dB/oct
    Pole4, // 24 dB/oct
    LinearPhase
};

struct MultibandDynamicsBand {
    std::optional<MixCompressor> compressor;
    std::optional<MixGate> expander;
    float gain = 0.0f; // dB post-dynamics
    bool solo = false;
};

struct MixMultibandDynamics {
    std::vector<float> crossover_frequencies;
    std::vector<MultibandDynamicsBand> bands;
    CrossoverSlope crossover_slope = CrossoverSlope::Pole4;
};

// =============================================================================
// §3.5 Saturation
// =============================================================================

enum class TapeSpeed : std::uint8_t { Ips15, Ips30 };
enum class ConsoleType : std::uint8_t { Neve, SSL, API, Generic };

enum class SaturationTypeTag : std::uint8_t { Tape, Tube, Transformer, Console, Soft, Hard };

struct SaturationConfig {
    SaturationTypeTag type = SaturationTypeTag::Tape;
    TapeSpeed tape_speed = TapeSpeed::Ips30;
    float tape_bias = 0.0f;
    std::string tube_model;
    ConsoleType console_type = ConsoleType::Generic;
};

struct MixSaturation {
    SaturationConfig algorithm;
    float drive = 0.0f;        // 0.0–1.0
    float mix = 1.0f;          // dry/wet
    float output_level = 0.0f; // dB gain compensation
};

// =============================================================================
// §3.6 Stereo Processing
// =============================================================================

struct MixStereoProcessor {
    float width = 1.0f;              // 0.0=mono, 1.0=original, >1.0=widened
    float mid_side_balance = 0.5f;   // 0.0=mid only, 0.5=equal, 1.0=side only
    std::optional<float> mono_below; // Hz; collapse below this to mono
};

// =============================================================================
// §3.7 Mix-stage delay and reverberation
// =============================================================================

enum class MixDelayMode : std::uint8_t { Mono, Stereo, PingPong };

struct MixDelay {
    bool tempo_synced = false;
    float delay_ms = 500.0f;
    Beat beat_division{1, 4};
    float feedback = 0.3f;
    MixDelayMode stereo_mode = MixDelayMode::Mono;
    float stereo_offset = 0.0f; // ms, or beat offset when tempo-synchronised
    float low_cut_hz = 80.0f;
    float high_cut_hz = 12000.0f;
    float modulation_rate = 0.0f;  // Hz
    float modulation_depth = 0.0f; // ms
    float mix = 1.0f;              // 0.0=dry, 1.0=wet
};

enum class MixReverbAlgorithm : std::uint8_t {
    Algorithmic,
    Convolution,
    Plate,
    Spring,
    Chamber,
    Hall,
    Room,
    Shimmer
};

struct MixReverb {
    MixReverbAlgorithm algorithm = MixReverbAlgorithm::Algorithmic;
    std::string impulse_response;
    float shimmer_pitch = 12.0f; // semitones
    float decay_time = 1.5f;     // RT60, seconds
    float pre_delay = 20.0f;     // ms
    float damping = 0.5f;
    float diffusion = 0.7f;
    float size = 0.5f;
    float early_reflections_level = 0.5f;
    float low_cut_hz = 80.0f;
    float high_cut_hz = 12000.0f;
    float mix = 1.0f; // 0.0=dry, 1.0=wet
};

// =============================================================================
// §3.8 Target DeviceParameter mapping
// =============================================================================

/**
 * Explicit conversion from one numeric MixEffect parameter to the inserted
 * target device's public Live DeviceParameter domain.
 *
 * The mapping is attached to its MixEffect, so target device identity is the
 * exact device inserted for that effect rather than a caller-guessed chain
 * index. The map key is the effect-relative numeric source path.
 */
struct MixDeviceParameter {
    std::string parameter_name;
    float range_min = 0.0f;
    float range_max = 1.0f;
    MappingCurve curve;
    float source_min = 0.0f;
    float source_max = 1.0f;
    DeviceParameterValueProperty value_property = DeviceParameterValueProperty::InternalValue;
};

// =============================================================================
// §3.2 MixEffect (sum type over all mix-stage processors)
// =============================================================================

using MixEffectParameters = std::variant<MixEQ,
                                         MixCompressor,
                                         MixGate,
                                         MixLimiter,
                                         MixMultibandDynamics,
                                         MixSaturation,
                                         MixStereoProcessor,
                                         MixDelay,
                                         MixReverb>;

struct MixEffect {
    MixEffectId id{};
    MixEffectParameters parameters;
    bool enabled = true;
    std::map<std::string, MixDeviceParameter> parameter_map;

    MixEffect() = default;
    MixEffect(MixEffectId effect_id,
              MixEffectParameters effect_parameters,
              bool is_enabled = true,
              std::map<std::string, MixDeviceParameter> mappings = {})
        : id(effect_id), parameters(std::move(effect_parameters)), enabled(is_enabled),
          parameter_map(std::move(mappings)) {}
};

// =============================================================================
// §3.1 MixEffectChain
// =============================================================================

struct MixEffectChain {
    std::vector<MixEffect> effects;
};

// =============================================================================
// §6.2 SpatialPosition
// =============================================================================

enum class PanLaw : std::uint8_t {
    ConstantPower, // -3 dB at centre
    Linear,        // -6 dB at centre
    ConstantPowerSinCos,
    Custom
};

enum class SpatialMode : std::uint8_t { SimplePan, BalancePan, BinauralHRTF, ObjectBased };

struct SpatialPosition {
    float pan = 0.0f;       // -1.0 to +1.0
    float depth = 0.0f;     // 0.0 to 1.0
    float elevation = 0.0f; // -1.0 to +1.0
    float width = 0.0f;     // 0.0=point, 1.0=full stereo
    PanLaw pan_law = PanLaw::ConstantPower;
    SpatialMode spatial_mode = SpatialMode::SimplePan;
    float custom_center_attenuation = -3.0f; // dB; for Custom pan law
    std::uint32_t object_id = 0;             // For ObjectBased spatial mode
};

// =============================================================================
// §6.3 Spatial Automation
// =============================================================================

struct SpatialBreakpoint {
    ScoreTime time;
    SpatialPosition position;
};

enum class InterpolationMode : std::uint8_t { Step, Linear, Smooth, Exponential };

struct SpatialAutomation {
    ChannelStripId channel_id{};
    std::vector<SpatialBreakpoint> breakpoints;
    InterpolationMode interpolation = InterpolationMode::Linear;
};

// =============================================================================
// §4.1 GroupBus routing
// =============================================================================

enum class GroupOutputType : std::uint8_t { Master, Group };

struct GroupOutput {
    GroupOutputType type = GroupOutputType::Master;
    GroupBusId parent_group_id{}; // For Group routing
};

struct GroupIntent {
    std::string function;
    std::string internal_balance_description;
};

// =============================================================================
// §5.1 AuxBus routing
// =============================================================================

enum class AuxOutputType : std::uint8_t { Master, Group };

struct AuxOutput {
    AuxOutputType type = AuxOutputType::Master;
    GroupBusId group_id{}; // For Group routing
};

// =============================================================================
// §7.3 Loudness
// =============================================================================

enum class LoudnessStandard : std::uint8_t {
    StreamingLoud,    // -14 LUFS, -1.0 dBFS peak
    StreamingDynamic, // -16 LUFS, -1.0 dBFS peak
    Broadcast,        // -23 LUFS, -1.0 dBFS peak (EBU R128)
    Film,             // -24 LUFS, -2.0 dBFS peak
    Vinyl,            // -12 to -9 LUFS, -0.5 dBFS peak
    Custom
};

struct LoudnessTarget {
    float integrated_lufs = -14.0f;
    float true_peak_dbfs = -1.0f;
    std::optional<float> loudness_range_lu;
    LoudnessStandard standard = LoudnessStandard::StreamingLoud;
};

// =============================================================================
// §7.4 Dithering
// =============================================================================

enum class DitherAlgorithm : std::uint8_t { TPDF, NoiseShaping, PowR };

struct DitheringConfig {
    std::uint8_t target_bit_depth = 16;
    DitherAlgorithm algorithm = DitherAlgorithm::TPDF;
    std::uint8_t noise_shaping_order = 3; // For NoiseShaping
    std::uint8_t pow_r_level = 2;         // For PowR
    bool auto_blank = true;
};

// =============================================================================
// §7.5 Metering
// =============================================================================

struct MeteringConfig {
    bool lufs = true;
    bool true_peak = true;
    bool rms = false;
    bool correlation = true;
    bool spectrum = false;
    bool dynamics = false;
};

// =============================================================================
// §8 Reference Profile System
// =============================================================================

struct BandEnergy {
    float low_hz = 0.0f;
    float high_hz = 0.0f;
    float energy_db = 0.0f;
};

struct SpectralProfile {
    std::vector<std::pair<float, float>> average_spectrum; // (Hz, dB)
    float spectral_centroid = 0.0f;                        // Hz
    float spectral_tilt = 0.0f;                            // dB/octave
    std::vector<BandEnergy> band_energies;
};

struct DynamicProfile {
    float crest_factor = 0.0f;   // dB
    float loudness_range = 0.0f; // LU
    float dynamic_range_dr = 0.0f;
    float peak_to_loudness_ratio = 0.0f; // dB
};

struct LoudnessProfile {
    float integrated = 0.0f;     // LUFS
    float short_term_max = 0.0f; // LUFS
    float momentary_max = 0.0f;  // LUFS
    float true_peak = 0.0f;      // dBFS
};

struct SpatialProfile {
    float average_correlation = 1.0f; // -1.0 to +1.0
    float average_width = 0.0f;       // 0.0–1.0
    float mid_side_ratio = 1.0f;
    float low_frequency_mono_coherence = 1.0f;
};

// =============================================================================
// §9 Mix Automation
// =============================================================================

struct MixAutomationBreakpoint {
    ScoreTime time;
    float value = 0.0f;
};

struct MixAutomation {
    std::string target; // Dot-separated path (§9.2)
    std::vector<MixAutomationBreakpoint> breakpoints;
    InterpolationMode interpolation = InterpolationMode::Linear;
    std::optional<std::string> intent;
};

// =============================================================================
// §8.7 Reference Comparison
// =============================================================================

struct MixRecommendation {
    std::string target;
    std::string suggestion;
    float confidence = 0.0f; // 0.0–1.0
};

/**
 * Mix-minus-reference differences (§8.7). An absent value means the mix side
 * has not been measured or configured, so no difference exists to report;
 * zero would instead assert that mix and reference agree.
 */
struct ReferenceComparison {
    std::optional<std::vector<std::pair<float, float>>> spectral_deviation; // (Hz, dB diff)
    std::optional<float> loudness_difference;                               // LU
    std::optional<float> dynamic_range_difference;                          // LU
    std::optional<float> width_difference;
    std::vector<MixRecommendation> recommendations;
};

// =============================================================================
// §8.2 MixAnnotation
// =============================================================================

struct MixAnnotation {
    std::string scope;   // What this annotation applies to
    std::string content; // Description
};

} // namespace sunny::core
