/**
 * @file MITP001A.h
 * @brief Mix IR foundation types — identifiers, enumerations, value types
 *
 * Component: MITP001A
 * Domain: MI (Mix IR) | Category: TP (Types)
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include "../Score/SITP001A.h"
#include "../Timbre/TITP001A.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Identifiers (§1.2, §2.1, §3.2, §4.1, §5.1)
// =============================================================================

struct MixGraphTag;
struct ChannelStripTag;
struct GroupBusTag;
struct AuxBusTag;
struct MixEffectTag;
struct ReferenceProfileTag;

using MixGraphId        = Id<MixGraphTag>;
using ChannelStripId    = Id<ChannelStripTag>;
using GroupBusId        = Id<GroupBusTag>;
using AuxBusId          = Id<AuxBusTag>;
using MixEffectId       = Id<MixEffectTag>;
using ReferenceProfileId = Id<ReferenceProfileTag>;

// =============================================================================
// Error Codes (7000–7099)
// =============================================================================

namespace MixError {
    // Structural (X-rules)
    constexpr int MissingChannel       = 7000;  // X1: Part without ChannelStrip
    constexpr int SignalFlowCycle      = 7001;  // X2: DAG cycle detected
    constexpr int UnreachableMaster    = 7002;  // X3: Channel cannot reach master bus
    constexpr int NoInsertProcessing   = 7003;  // X4: Channel has no insert chain
    constexpr int SilentNotMuted       = 7004;  // X5: Fader at -inf but not muted
    constexpr int InvalidSidechain     = 7005;  // X6: Sidechain references non-existent source

    // Audio quality (A-rules)
    constexpr int MasterClipping       = 7010;  // A1: Master exceeds 0 dBFS true peak
    constexpr int LoudnessExceeded     = 7011;  // A2: Master exceeds target by >1 LU
    constexpr int LowCorrelation       = 7012;  // A3: Stereo correlation below 0.0
    constexpr int SubBassPhase         = 7013;  // A4: Sub-bass correlation below 0.5
    constexpr int LoudnessRangeWide    = 7014;  // A5: LRA exceeds reference by >3 LU
    constexpr int ChannelClipping      = 7015;  // A6: Channel clips before bus
    constexpr int SpectralDeviation    = 7016;  // A7: Spectral deviation >3 dB from ref

    // Intent (I-rules)
    constexpr int NoChannelIntent      = 7020;  // I1: No ChannelIntent annotation
    constexpr int NoGroupIntent        = 7021;  // I2: No GroupIntent annotation
    constexpr int LeadTooQuiet         = 7022;  // I3: Lead role below average level
    constexpr int FoundationNoSubBass  = 7023;  // I4: Foundation with HPF removing sub
    constexpr int FlatDepthStaging     = 7024;  // I5: All depth positions identical

    // General
    constexpr int InvalidParameter     = 7030;  // Parameter value rejected
    constexpr int NotFound             = 7031;  // Bus, channel, or effect not found
    constexpr int DuplicateId          = 7032;  // Duplicate identifier
    constexpr int InvalidPath          = 7033;  // Invalid automation target path
    constexpr int NestingDepthExceeded = 7034;  // Group bus nesting > max depth
}  // namespace MixError

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

enum class LevelReferenceType : std::uint8_t {
    MasterTarget,
    Channel,
    Group
};

struct LevelReference {
    LevelReferenceType type = LevelReferenceType::MasterTarget;
    float lufs = -14.0f;                     // For MasterTarget
    ChannelStripId channel_id{};             // For Channel
    std::string relationship;                // For Channel (e.g. "3 dB below Violin I")
    GroupBusId group_id{};                   // For Group
};

struct RelativeLevel {
    LevelReference reference;
    float offset_db = 0.0f;
};

struct Fader {
    float level_db = 0.0f;                  // dB; -inf to +12
    std::optional<RelativeLevel> relative_level;
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
    float fundamental_low = 0.0f;           // Hz
    float fundamental_high = 0.0f;          // Hz
    float presence_low = 0.0f;              // Hz
    float presence_high = 0.0f;             // Hz
    std::optional<float> avoid_low;         // Hz
    std::optional<float> avoid_high;        // Hz
};

enum class DepthPosition : std::uint8_t {
    FrontClose,
    FrontMid,
    Mid,
    MidFar,
    Far,
    VeryFar
};

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
    float threshold = -20.0f;               // dB
    float ratio = 2.0f;
    float attack = 10.0f;                   // ms
    float release = 100.0f;                 // ms
};

enum class MixEQBandType : std::uint8_t {
    Peak,
    LowShelf,
    HighShelf,
    LowCut,
    HighCut,
    TiltShelf
};

struct MixEQBand {
    float frequency = 1000.0f;              // Hz
    float gain = 0.0f;                      // dB
    float q = 1.0f;
    MixEQBandType band_type = MixEQBandType::Peak;
    std::optional<DynamicEQConfig> dynamic;
};

struct MixEQ {
    std::vector<MixEQBand> bands;           // Up to 8 bands
    bool linear_phase = false;
    bool auto_gain = false;
};

// =============================================================================
// §3.4 Mix-Stage Dynamics
// =============================================================================

enum class DetectionMode : std::uint8_t { Peak, RMS, Envelope };

enum class CompressorTopology : std::uint8_t { FeedForward, FeedBack };

enum class SidechainSourceType : std::uint8_t {
    Internal,
    ExternalChannel,
    ExternalBus
};

struct SidechainFilter {
    MixEQBandType filter_type = MixEQBandType::HighCut;
    float frequency = 1000.0f;              // Hz
    float q = 1.0f;
};

struct MixSidechainConfig {
    SidechainSourceType source = SidechainSourceType::Internal;
    ChannelStripId channel_id{};            // For ExternalChannel
    GroupBusId bus_id{};                     // For ExternalBus
    std::optional<SidechainFilter> filter;
};

struct MixCompressor {
    float threshold = -20.0f;               // dB
    float ratio = 4.0f;
    float attack = 10.0f;                   // ms
    float release = 100.0f;                 // ms
    float knee = 0.0f;                      // dB (0 = hard knee)
    float makeup_gain = 0.0f;               // dB
    DetectionMode detection = DetectionMode::RMS;
    CompressorTopology topology = CompressorTopology::FeedForward;
    MixSidechainConfig sidechain;
    float stereo_link = 1.0f;              // 0.0 = independent, 1.0 = linked
};

struct MixGate {
    float threshold = -40.0f;               // dB
    float ratio = 10.0f;                    // Expansion ratio
    float attack = 0.5f;                    // ms
    float hold = 50.0f;                     // ms
    float release = 100.0f;                 // ms
    float range = -80.0f;                   // dB (max attenuation)
    MixSidechainConfig sidechain;
};

enum class LimiterAlgorithm : std::uint8_t {
    Brickwall,
    TruePeak,
    ISP
};

struct MixLimiter {
    float ceiling = -1.0f;                  // dBFS
    float release = 100.0f;                 // ms
    float lookahead = 5.0f;                 // ms
    LimiterAlgorithm algorithm = LimiterAlgorithm::TruePeak;
};

enum class CrossoverSlope : std::uint8_t {
    Pole2,          // 12 dB/oct
    Pole4,          // 24 dB/oct
    LinearPhase
};

struct MultibandDynamicsBand {
    std::optional<MixCompressor> compressor;
    std::optional<MixGate> expander;
    float gain = 0.0f;                     // dB post-dynamics
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

enum class SaturationTypeTag : std::uint8_t {
    Tape,
    Tube,
    Transformer,
    Console,
    Soft,
    Hard
};

struct SaturationConfig {
    SaturationTypeTag type = SaturationTypeTag::Tape;
    TapeSpeed tape_speed = TapeSpeed::Ips30;
    float tape_bias = 0.0f;
    std::string tube_model;
    ConsoleType console_type = ConsoleType::Generic;
};

struct MixSaturation {
    SaturationConfig algorithm;
    float drive = 0.0f;                    // 0.0–1.0
    float mix = 1.0f;                      // dry/wet
    float output_level = 0.0f;             // dB gain compensation
};

// =============================================================================
// §3.6 Stereo Processing
// =============================================================================

struct MixStereoProcessor {
    float width = 1.0f;                    // 0.0=mono, 1.0=original, >1.0=widened
    float mid_side_balance = 0.5f;         // 0.0=mid only, 0.5=equal, 1.0=side only
    std::optional<float> mono_below;       // Hz; collapse below this to mono
};

// =============================================================================
// §3.2 MixEffect (sum type over all mix-stage processors)
// =============================================================================

using MixEffectParameters = std::variant<
    MixEQ,
    MixCompressor,
    MixGate,
    MixLimiter,
    MixMultibandDynamics,
    MixSaturation,
    MixStereoProcessor
>;

struct MixEffect {
    MixEffectId id{};
    MixEffectParameters parameters;
    bool enabled = true;
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
    ConstantPower,          // -3 dB at centre
    Linear,                 // -6 dB at centre
    ConstantPowerSinCos,
    Custom
};

enum class SpatialMode : std::uint8_t {
    SimplePan,
    BalancePan,
    BinauralHRTF,
    ObjectBased
};

struct SpatialPosition {
    float pan = 0.0f;                      // -1.0 to +1.0
    float depth = 0.0f;                    // 0.0 to 1.0
    float elevation = 0.0f;                // -1.0 to +1.0
    float width = 0.0f;                    // 0.0=point, 1.0=full stereo
    PanLaw pan_law = PanLaw::ConstantPower;
    SpatialMode spatial_mode = SpatialMode::SimplePan;
    float custom_center_attenuation = -3.0f; // dB; for Custom pan law
    std::uint32_t object_id = 0;           // For ObjectBased spatial mode
};

// =============================================================================
// §6.3 Spatial Automation
// =============================================================================

struct SpatialBreakpoint {
    ScoreTime time;
    SpatialPosition position;
};

enum class InterpolationMode : std::uint8_t {
    Step,
    Linear,
    Smooth,
    Exponential
};

struct SpatialAutomation {
    ChannelStripId channel_id{};
    std::vector<SpatialBreakpoint> breakpoints;
    InterpolationMode interpolation = InterpolationMode::Linear;
};

// =============================================================================
// §4.1 GroupBus routing
// =============================================================================

enum class GroupOutputType : std::uint8_t {
    Master,
    Group
};

struct GroupOutput {
    GroupOutputType type = GroupOutputType::Master;
    GroupBusId parent_group_id{};           // For Group routing
};

struct GroupIntent {
    std::string function;
    std::string internal_balance_description;
};

// =============================================================================
// §5.1 AuxBus routing
// =============================================================================

enum class AuxOutputType : std::uint8_t {
    Master,
    Group
};

struct AuxOutput {
    AuxOutputType type = AuxOutputType::Master;
    GroupBusId group_id{};                  // For Group routing
};

// =============================================================================
// §7.3 Loudness
// =============================================================================

enum class LoudnessStandard : std::uint8_t {
    StreamingLoud,          // -14 LUFS, -1.0 dBFS peak
    StreamingDynamic,       // -16 LUFS, -1.0 dBFS peak
    Broadcast,              // -23 LUFS, -1.0 dBFS peak (EBU R128)
    Film,                   // -24 LUFS, -2.0 dBFS peak
    Vinyl,                  // -12 to -9 LUFS, -0.5 dBFS peak
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

enum class DitherAlgorithm : std::uint8_t {
    TPDF,
    NoiseShaping,
    PowR
};

struct DitheringConfig {
    std::uint8_t target_bit_depth = 16;
    DitherAlgorithm algorithm = DitherAlgorithm::TPDF;
    std::uint8_t noise_shaping_order = 3;   // For NoiseShaping
    std::uint8_t pow_r_level = 2;           // For PowR
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
    std::vector<std::pair<float, float>> average_spectrum;   // (Hz, dB)
    float spectral_centroid = 0.0f;                          // Hz
    float spectral_tilt = 0.0f;                              // dB/octave
    std::vector<BandEnergy> band_energies;
};

struct DynamicProfile {
    float crest_factor = 0.0f;              // dB
    float loudness_range = 0.0f;            // LU
    float dynamic_range_dr = 0.0f;
    float peak_to_loudness_ratio = 0.0f;    // dB
};

struct LoudnessProfile {
    float integrated = 0.0f;                // LUFS
    float short_term_max = 0.0f;            // LUFS
    float momentary_max = 0.0f;             // LUFS
    float true_peak = 0.0f;                 // dBFS
};

struct SpatialProfile {
    float average_correlation = 1.0f;       // -1.0 to +1.0
    float average_width = 0.0f;             // 0.0–1.0
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
    std::string target;                     // Dot-separated path (§9.2)
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
    float confidence = 0.0f;               // 0.0–1.0
};

struct ReferenceComparison {
    std::vector<std::pair<float, float>> spectral_deviation;   // (Hz, dB diff)
    float loudness_difference = 0.0f;       // LUFS
    float dynamic_range_difference = 0.0f;  // LU
    float width_difference = 0.0f;
    std::vector<MixRecommendation> recommendations;
};

// =============================================================================
// §10.2 Device Mapping (compilation config)
// =============================================================================

struct MixDeviceMapping {
    std::string mix_effect_type;            // "EQ", "Compressor", etc.
    std::string ableton_device;             // Native device name
    std::optional<std::string> plugin_alternative;
};

// =============================================================================
// §8.2 MixAnnotation
// =============================================================================

struct MixAnnotation {
    std::string scope;                      // What this annotation applies to
    std::string content;                    // Description
};

}  // namespace Sunny::Core

#pragma GCC diagnostic pop
