/**
 * @file workflows.hpp
 * @brief Mix IR workflow functions — agent-driven mix configuration
 *
 *
 * Provides entry-point functions for agent-driven mixing workflows per
 * Mix Spec §12. These functions create, modify, and query MixGraph
 * objects at the compositional level.
 *
 * Functions divide into four categories:
 *   Construction: create_mix_graph, create_group_bus, create_aux_bus
 *   Mutation:     channel/bus configuration, effect chains, routing
 *   Query:        parameter access, reference comparison
 *   Validation:   thin wrapper over
 *
 * Invariants:
 * - Mutations leave the graph in a structurally valid state
 * - Parameter paths follow dot-separated segment notation
 * - Orchestral seating templates apply standard pan/depth positions
 */

#pragma once

#include <sunny/core/mix/document.hpp>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/mix/validation.hpp>

namespace sunny::core {

// =============================================================================
// Graph Construction (§12.1: create_mix_graph)
// =============================================================================

/**
 * @brief Create a new MixGraph with one ChannelStrip per Part.
 *
 * Initialises each channel with default fader at 0 dB, centre pan,
 * no insert processing, and no group assignment.
 */
[[nodiscard]] MixGraph create_mix_graph(MixGraphId id, const std::vector<PartId>& part_ids);

// =============================================================================
// Group Bus (§12.1: create_group_bus, assign_channel_to_group)
// =============================================================================

/**
 * @brief Create a group bus and establish exact mirrored channel membership.
 *
 * Rejects missing or duplicate requested channel IDs without mutating the graph.
 * Channels already assigned elsewhere are moved to the new group.
 */
[[nodiscard]] Result<void> create_group_bus(MixGraph& graph,
                                            GroupBusId id,
                                            const std::string& name,
                                            const std::vector<ChannelStripId>& member_channels);

/**
 * @brief Assign a channel to exactly one group bus, repairing stale reverse edges.
 */
[[nodiscard]] Result<void>
assign_channel_to_group(MixGraph& graph, ChannelStripId channel_id, GroupBusId group_id);

/**
 * @brief Route one GroupBus into another with exact mirrored nesting membership.
 *
 * The mutation is transactional and rejects cycles, excessive nesting, missing
 * buses, and any resulting structural error.
 */
[[nodiscard]] Result<void>
assign_group_to_group(MixGraph& graph, GroupBusId child_group_id, GroupBusId parent_group_id);

/** Route a GroupBus directly to Master and remove every stale parent membership edge. */
[[nodiscard]] Result<void> route_group_to_master(MixGraph& graph, GroupBusId group_id);

// =============================================================================
// Aux Bus (§12.1: create_aux_bus, set_channel_send)
// =============================================================================

/**
 * @brief Create an auxiliary bus with a name and empty effect chain.
 */
[[nodiscard]] Result<void> create_aux_bus(MixGraph& graph, AuxBusId id, const std::string& name);

/**
 * @brief Set a channel's send level to an aux bus.
 */
[[nodiscard]] Result<void> set_channel_send(MixGraph& graph,
                                            ChannelStripId channel_id,
                                            AuxBusId aux_id,
                                            float level_db,
                                            bool pre_fader = false);

// =============================================================================
// Effect Chain (§12.1: add_channel_effect, add_bus_effect, add_master_effect)
// =============================================================================

/**
 * @brief Add a mix effect to a channel's insert chain.
 */
[[nodiscard]] Result<void>
add_channel_effect(MixGraph& graph, ChannelStripId channel_id, MixEffect effect);

/**
 * @brief Add a mix effect to a group bus's insert chain.
 */
[[nodiscard]] Result<void> add_bus_effect(MixGraph& graph, GroupBusId bus_id, MixEffect effect);

/**
 * @brief Add a mix effect to an aux bus's effect chain.
 */
[[nodiscard]] Result<void> add_aux_effect(MixGraph& graph, AuxBusId aux_id, MixEffect effect);

/**
 * @brief Add a mix effect to the master bus chain.
 */
void add_master_effect(MixGraph& graph, MixEffect effect);

/** Read one numeric, boolean, or enum MixEffect parameter as a scalar. */
[[nodiscard]] Result<float> get_mix_effect_parameter(const MixEffect& effect,
                                                     const std::string& path);

/** Enumerate every presently materialised scalar source path for an effect. */
[[nodiscard]] std::vector<std::string> mix_effect_parameter_paths(const MixEffect& effect);

/** Enumerate materialised non-scalar fields outside DeviceParameter mapping. */
[[nodiscard]] std::vector<std::string> mix_effect_non_scalar_paths(const MixEffect& effect);

/** Apply an explicit source-domain/curve/target-domain mapping. */
[[nodiscard]] Result<float> map_mix_device_parameter_value(float source_value,
                                                           const MixDeviceParameter& mapping);

/**
 * Attach or replace a target mapping on the uniquely identified effect.
 * The mapping and current source value are validated before mutation.
 */
[[nodiscard]] Result<void> map_mix_effect_parameter(MixGraph& graph,
                                                    MixEffectId effect_id,
                                                    const std::string& source_path,
                                                    MixDeviceParameter mapping);

// =============================================================================
// Level and Spatial (§12.1: set_channel_level, set_channel_pan, etc.)
// =============================================================================

/**
 * @brief Set a channel's fader level (absolute dB).
 */
[[nodiscard]] Result<void>
set_channel_level(MixGraph& graph, ChannelStripId channel_id, float level_db);

/**
 * @brief Set a channel's fader level relative to another channel.
 */
[[nodiscard]] Result<void> set_channel_relative_level(MixGraph& graph,
                                                      ChannelStripId channel_id,
                                                      const RelativeLevel& relative);

/**
 * Resolve the complete channel/group/master fader dependency graph.
 *
 * Channel and Group references are exact additive dB constraints. A
 * MasterTarget reference remains explicitly unresolved until programme
 * loudness is measured. Missing references, cycles, non-finite values, and
 * derived values above the model's +12 dB ceiling are errors.
 */
[[nodiscard]] Result<RelativeLevelResolution> resolve_relative_levels(const MixGraph& graph);

/**
 * @brief Set a channel's spatial position.
 */
[[nodiscard]] Result<void>
set_channel_spatial(MixGraph& graph, ChannelStripId channel_id, SpatialPosition spatial);

/**
 * @brief Set a channel's pan position (-1.0 to +1.0).
 */
[[nodiscard]] Result<void> set_channel_pan(MixGraph& graph, ChannelStripId channel_id, float pan);

// =============================================================================
// Intent (§12.1: set_channel_intent, set_group_intent)
// =============================================================================

/**
 * @brief Set the mixing intent for a channel.
 */
[[nodiscard]] Result<void>
set_channel_intent(MixGraph& graph, ChannelStripId channel_id, ChannelIntent intent);

/**
 * @brief Set the mixing intent for a group bus.
 */
[[nodiscard]] Result<void>
set_group_intent(MixGraph& graph, GroupBusId group_id, GroupIntent intent);

// =============================================================================
// Loudness and Output (§12.1: set_loudness_target, set_output_format)
// =============================================================================

/**
 * @brief Set the master bus loudness target.
 */
void set_loudness_target(MixGraph& graph, LoudnessTarget target);

/**
 * @brief Set the output format (stereo, surround, etc.)
 */
void set_output_format(MixGraph& graph, OutputFormat format);

// =============================================================================
// Automation (§12.1: add_mix_automation)
// =============================================================================

/**
 * @brief Add parameter automation to the mix graph.
 */
void add_automation(MixGraph& graph, MixAutomation automation);

// =============================================================================
// Reference Profiles (§12.1: create_reference_profile, compare_to_reference)
// =============================================================================

/**
 * @brief Add a reference profile to the graph.
 */
void add_reference_profile(MixGraph& graph, ReferenceProfile profile);

/**
 * @brief Compare current mix parameters against a reference profile.
 *
 * Performs static comparison of configured values (loudness target,
 * spectral shape from EQ curves) against caller-supplied reference
 * measurements. Sunny currently performs no runtime audio analysis.
 */
[[nodiscard]] Result<ReferenceComparison> compare_to_reference(const MixGraph& graph,
                                                               ReferenceProfileId ref_id);

// =============================================================================
// Orchestral Seating Templates (§6.4)
// =============================================================================

enum class SeatingTemplate : std::uint8_t {
    American, // Stokowski
    European  // Traditional
};

/**
 * @brief Apply an orchestral seating template to channel spatial positions.
 *
 * Maps instrument families to pan/depth positions based on the selected
 * arrangement. Requires channels to have ChannelIntent with frequency
 * allocation data for mapping.
 */
void apply_seating_template(MixGraph& graph, SeatingTemplate seating);

// =============================================================================
// Validation (§12.1: validate_mix)
// =============================================================================

/**
 * @brief Run full validation on the mix graph. Thin wrapper over.
 */
[[nodiscard]] std::vector<Diagnostic> validate(const MixGraph& graph);

} // namespace sunny::core
