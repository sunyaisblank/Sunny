/**
 * @file ableton_mix.hpp
 * @brief Ableton Live Compiler — Mix IR to mixer infrastructure
 *
 *
 * Compiles a validated MixGraph into LOM commands that configure
 * Ableton's mixer: return tracks, native effects, send levels, and
 * fader/pan/mute states and the neutral Main output stage.
 *
 * Compilation steps (Mix IR Spec §10):
 *   1. Validate and resolve relative fader constraints/residuals
 *   2. Report GroupBuses that LOM cannot create
 *   3. Create return tracks for AuxBuses
 *   4. Insert effect chains on channels, groups, aux, and master
 *   5. Configure supported send routing
 *   6. Set resolved/fallback levels, pan, mute/solo, and Track Activators
 *   7. Insert the master processing chain and neutralise Main activator/pan
 *   8. Report automation lanes that LOM cannot author
 *
 * Precondition:  MixGraph passes validation (no Error diagnostics)
 * Postcondition: Supported mixer infrastructure is created via transport and
 * any unsupported requested operations are reported as warnings.
 */

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <sunny/core/mix/document.hpp>
#include <sunny/infrastructure/formats/ableton_evidence.hpp>
#include <vector>

namespace sunny::infrastructure::formats {

using AbletonPartTrackMap = std::map<sunny::core::PartId, int>;

/** One exact target-owned Track routing dictionary. Both strings remain opaque. */
struct AbletonRoutingOptionBinding {
    std::string display_name;
    std::string identifier;

    friend bool operator==(const AbletonRoutingOptionBinding&,
                           const AbletonRoutingOptionBinding&) = default;
};

/**
 * An explicit semantic admission from one Sunny edge to two exact dictionaries
 * returned by a named Live target. The provenance is retained as an assumption,
 * never inferred from either target-owned string.
 */
struct AbletonOutputRouteBinding {
    AbletonRoutingOptionBinding type;
    AbletonRoutingOptionBinding channel;
    std::string mapping_provenance;

    friend bool operator==(const AbletonOutputRouteBinding&,
                           const AbletonOutputRouteBinding&) = default;
};

/** Bindings exist only for materialisable Part Tracks and Aux Return Tracks. */
struct AbletonOutputRoutingBindings {
    std::map<sunny::core::PartId, AbletonOutputRouteBinding> part_tracks;
    std::map<sunny::core::AuxBusId, AbletonOutputRouteBinding> aux_returns;

    friend bool operator==(const AbletonOutputRoutingBindings&,
                           const AbletonOutputRoutingBindings&) = default;
};

enum class AbletonOutputRouteAction : std::uint8_t { NotApplied, RecordedOnly, Set };

[[nodiscard]] const char* ableton_output_route_action_name(AbletonOutputRouteAction action);

struct MixCompilationResult {
    struct OutputRouteDeployment {
        std::string source;
        std::string destination;
        std::string target_path;
        std::optional<sunny::core::PartId> part_id;
        std::optional<sunny::core::AuxBusId> aux_bus_id;
        std::optional<AbletonOutputRouteBinding> binding;
        std::optional<AbletonRoutingOptionBinding> observed_type_after_type_stage;
        std::optional<AbletonRoutingOptionBinding> observed_type;
        std::optional<AbletonRoutingOptionBinding> observed_channel;
        std::vector<AbletonRoutingOptionBinding> available_types;
        std::vector<AbletonRoutingOptionBinding> available_channels;
        bool requested_type_available_verified = false;
        bool type_stage_verified = false;
        bool requested_channel_available_verified = false;
        bool channel_stage_verified = false;
        bool final_membership_verified = false;
        bool verified = false;
        AbletonOutputRouteAction action = AbletonOutputRouteAction::NotApplied;
    };

    struct ReturnTrackDeployment {
        sunny::core::AuxBusId aux_bus_id{};
        int track_index = 0;
        std::string requested_name;
        bool requested_mute = false;
        bool requested_solo = false;
        double requested_track_activator = 1.0;
        int requested_crossfade_assign = 1;
        int requested_panning_mode = 0;
        float requested_pan = 0.0f;
    };

    struct MasterTrackDeployment {
        double requested_track_activator = 1.0;
        int requested_panning_mode = 0;
        double requested_pan = 0.0;
    };

    AbletonTargetProfile target_profile;
    std::uint64_t group_tracks_requested = 0;
    std::uint64_t group_tracks_created = 0;
    std::uint64_t return_tracks_requested = 0;
    std::uint64_t return_tracks_created = 0;
    std::uint64_t effects_requested = 0;
    std::uint64_t effects_inserted = 0;
    std::uint64_t effects_verified = 0;
    std::uint64_t sends_requested = 0;
    /** Fully configured sends (level plus requested pre/post-fader mode). */
    std::uint64_t sends_configured = 0;
    std::uint64_t send_levels_requested = 0;
    std::uint64_t send_levels_configured = 0;
    std::uint64_t send_modes_requested = 0;
    std::uint64_t send_modes_configured = 0;
    std::uint64_t output_routes_requested = 0;
    std::uint64_t output_routes_written = 0;
    std::uint64_t output_routes_verified = 0;
    std::uint64_t automation_lanes_requested = 0;
    std::uint64_t automation_lanes_written = 0;
    std::uint64_t channels_requested = 0;
    std::uint64_t channels_configured = 0;
    std::uint64_t property_writes = 0;
    std::uint64_t property_writes_verified = 0;
    std::uint64_t parameter_sources_total = 0;
    std::uint64_t parameter_sources_explicitly_mapped = 0;
    std::uint64_t parameter_sources_unmapped = 0;
    std::uint64_t parameters_mapped = 0;
    std::uint64_t parameters_verified = 0;
    sunny::core::RelativeLevelResolution fader_level_resolution;
    std::vector<AbletonPropertyDeployment> property_deployments;
    /** Exact generated Return Track identities and their tractable gain-stage intent. */
    std::vector<ReturnTrackDeployment> return_track_deployments;
    /** Exact neutral projection for MasterBus properties absent from the source IR. */
    std::optional<MasterTrackDeployment> master_track_deployment;
    std::vector<AbletonDeviceInsertionDeployment> device_deployments;
    struct ParameterDeployment {
        sunny::core::MixEffectId effect_id{};
        std::string source_path;
        std::string device_path;
        std::string parameter_name;
        sunny::core::DeviceParameterValueProperty value_property =
            sunny::core::DeviceParameterValueProperty::InternalValue;
        float source_value = 0.0f;
        float requested_value = 0.0f;
        float range_min = 0.0f;
        float range_max = 1.0f;
        std::optional<std::string> matched_name;
        std::optional<std::string> original_name;
        std::optional<float> observed_value;
        std::optional<float> observed_minimum;
        std::optional<float> observed_maximum;
        std::optional<bool> is_quantized;
        std::optional<float> default_value;
        std::optional<std::vector<std::string>> value_items;
        std::optional<bool> is_enabled;
        bool verified = false;
        std::optional<std::uint8_t> parameter_state;
        std::optional<std::uint8_t> automation_state;
        AbletonParameterAction action = AbletonParameterAction::NotApplied;
    };
    std::vector<ParameterDeployment> parameter_deployments;
    struct ParameterCoverage {
        sunny::core::MixEffectId effect_id{};
        std::uint64_t source_parameters = 0;
        std::uint64_t explicitly_mapped = 0;
        bool target_deployable = false;
        std::vector<std::string> missing_paths;
        std::vector<std::string> non_parameter_residual_paths;
    };
    std::vector<ParameterCoverage> parameter_coverage;
    std::vector<OutputRouteDeployment> output_route_deployments;
    std::vector<std::string> output_route_residuals;
    std::vector<std::string> warnings;
};

/// Live's mixer fader ceiling: DeviceParameter.max of a track volume is +6 dB.
inline constexpr float ABLETON_FADER_CEILING_DB = 6.0f;

/// Live's send ceiling: a send knob's maximum is 0 dB.
inline constexpr float ABLETON_SEND_CEILING_DB = 0.0f;

/**
 * @brief List every level the compiler would write that Live cannot represent.
 *
 * Covers the channel, master and return-track faders the compiler writes
 * (resolved relative levels or their explicit fallbacks) against
 * ABLETON_FADER_CEILING_DB, and enabled channel sends against
 * ABLETON_SEND_CEILING_DB. Group faders and group sends are not
 * materialised and are not checked. An empty result means every written
 * level is representable. compile_mix_to_ableton refuses a non-empty result
 * with TargetValueUnrepresentable before its first target mutation.
 *
 * @pre graph is valid (its relative fader levels resolve)
 */
[[nodiscard]] sunny::core::Result<std::vector<std::string>>
ableton_unrepresentable_mix_levels(const sunny::core::MixGraph& graph);

/**
 * @brief Compile channels to explicit PartId-bound Ableton tracks
 *
 * MixGraph channel order has no effect on target selection. Every channel
 * PartId must have one non-negative mapping, and two parts may not target the
 * same Live track. Live deployment reaches this through the guarded project
 * path, which binds part_tracks from a captured target snapshot.
 */
[[nodiscard]] sunny::core::Result<MixCompilationResult>
compile_mix_to_ableton(const sunny::core::MixGraph& graph,
                       const AbletonPartTrackMap& part_tracks,
                       LomTransport& transport);

[[nodiscard]] sunny::core::Result<MixCompilationResult>
compile_mix_to_ableton(const sunny::core::MixGraph& graph,
                       const AbletonPartTrackMap& part_tracks,
                       LomTransport& transport,
                       const AbletonOutputRoutingBindings& routing_bindings);

} // namespace sunny::infrastructure::formats
