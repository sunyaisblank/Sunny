/**
 * @file MIDC001A.h
 * @brief Mix IR document model — MixGraph, ChannelStrip, GroupBus, AuxBus, MasterBus
 *
 * Component: MIDC001A
 * Domain: MI (Mix IR) | Category: DC (Document)
 *
 * Defines the complete document model for the Mix IR: the MixGraph root object,
 * ChannelStrip (one per Score IR Part), GroupBus (collective processing),
 * AuxBus (parallel effects), MasterBus (final output), and ReferenceProfile.
 *
 * Composes with Mix IR foundation types (MITP001A) and Score IR types
 * (PartId, ScoreTime).
 *
 * Invariants:
 * - Every Score IR Part has exactly one ChannelStrip
 * - Signal flow graph is a DAG (no cycles)
 * - Every channel reaches the master bus through some path
 * - Group bus nesting does not exceed configurable max depth (default 4)
 */

#pragma once

#include "MITP001A.h"

namespace Sunny::Core {

// =============================================================================
// §2.1 ChannelStrip
// =============================================================================

/**
 * @brief Mixing controls for a single instrument channel.
 *
 * One ChannelStrip per Score IR Part. Receives the output of the Timbre IR's
 * insert chain and routes through mix-stage processing, fader, spatial
 * positioning, and group/aux send structure.
 */
struct ChannelStrip {
    ChannelStripId id{};
    PartId part_id{};
    float input_trim = 0.0f;               // dB (-24 to +24)
    bool polarity_invert = false;
    MixEffectChain insert_chain;
    Fader fader;
    SpatialPosition spatial;
    bool mute = false;
    bool solo = false;
    std::vector<AuxSendLevel> sends;
    std::optional<GroupBusId> group_assignment;
    std::optional<ChannelIntent> intent;
};

// =============================================================================
// §4.1 GroupBus
// =============================================================================

/**
 * @brief Sums multiple channels for collective processing.
 *
 * Channels feed into at most one group bus (single-parent constraint).
 * Group buses can nest: a "Woodwinds" group within an "Orchestral" group.
 */
struct GroupBus {
    GroupBusId id{};
    std::string name;
    std::vector<ChannelStripId> member_channels;
    std::vector<GroupBusId> member_groups;
    MixEffectChain insert_chain;
    Fader fader;
    SpatialPosition spatial;
    bool mute = false;
    bool solo = false;
    std::vector<AuxSendLevel> sends;
    GroupOutput output;
    std::optional<GroupIntent> intent;
};

// =============================================================================
// §5.1 AuxBus
// =============================================================================

/**
 * @brief Parallel effects processing shared across multiple channels.
 *
 * Channels send a copy of their signal (at a configurable level) to an
 * auxiliary bus, which processes it (typically reverb or delay) and returns
 * the processed signal to the mix.
 */
struct AuxBus {
    AuxBusId id{};
    std::string name;
    MixEffectChain effect_chain;
    float return_level = 0.0f;              // dB
    SpatialPosition return_spatial;
    AuxOutput output;
    std::optional<std::string> intent;
};

// =============================================================================
// §7.1 MasterBus
// =============================================================================

/**
 * @brief Final processing stage before output.
 *
 * All group buses, ungrouped channels, and aux returns converge here.
 * The master bus carries the master processing chain and outputs the
 * final stereo (or surround) signal.
 */
struct MasterBus {
    MixEffectChain insert_chain;
    Fader fader;
    OutputFormat output_format = OutputFormat::Stereo;
    std::optional<AtmosConfig> atmos_config;
    std::optional<DitheringConfig> dithering;
    std::optional<LoudnessTarget> target_loudness;
    MeteringConfig metering;
};

// =============================================================================
// §8.2 ReferenceProfile
// =============================================================================

/**
 * @brief Analysed characteristics of a reference mix for comparison.
 *
 * Mixing by reference is standard professional practice: engineers compare
 * their work against commercially released mixes to calibrate spectral
 * balance, dynamic range, loudness, and spatial characteristics.
 */
struct ReferenceProfile {
    ReferenceProfileId id{};
    std::string name;
    std::optional<std::string> source;
    SpectralProfile spectral_profile;
    DynamicProfile dynamic_profile;
    LoudnessProfile loudness_profile;
    SpatialProfile spatial_profile;
    std::vector<std::pair<float, float>> tonal_balance_curve;   // (Hz, dB)
};

// =============================================================================
// §1.2 MixGraph — root object
// =============================================================================

/**
 * @brief Root object of the Mix IR.
 *
 * Represents the complete mixing, spatial positioning, and mastering
 * configuration for a production. The signal flow forms a DAG:
 *
 *   ChannelStrip[] → GroupBus[] → MasterBus → Output
 *        │                │
 *        └→ AuxSend[] ────┘
 *             │
 *        AuxReturn[]
 *             │
 *             └→ GroupBus / MasterBus
 */
struct MixGraph {
    MixGraphId id{};
    std::vector<ChannelStrip> channels;
    std::vector<GroupBus> group_buses;
    std::vector<AuxBus> aux_buses;
    MasterBus master_bus;
    std::vector<MixAnnotation> mix_annotations;
    std::vector<ReferenceProfile> reference_profiles;
    std::vector<MixAutomation> automation;
    OutputFormat output_format = OutputFormat::Stereo;
    std::uint8_t max_group_nesting_depth = 4;
};

}  // namespace Sunny::Core
