/**
 * @file MIWF001A.h
 * @brief Mix IR workflow functions — agent-driven mix configuration
 *
 * Component: MIWF001A
 * Domain: MI (Mix IR) | Category: WF (Workflow)
 *
 * Provides entry-point functions for agent-driven mixing workflows per
 * Mix Spec §12. These functions create, modify, and query MixGraph
 * objects at the compositional level.
 *
 * Functions divide into four categories:
 *   Construction: create_mix_graph, create_group_bus, create_aux_bus
 *   Mutation:     channel/bus configuration, effect chains, routing
 *   Query:        parameter access, reference comparison
 *   Validation:   thin wrapper over MIVD001A
 *
 * Invariants:
 * - Mutations leave the graph in a structurally valid state
 * - Parameter paths follow dot-separated segment notation
 * - Orchestral seating templates apply standard pan/depth positions
 */

#pragma once

#include "MIDC001A.h"
#include "MIVD001A.h"
#include "MISZ001A.h"

namespace Sunny::Core {

// =============================================================================
// Graph Construction (§12.1: create_mix_graph)
// =============================================================================

/**
 * @brief Create a new MixGraph with one ChannelStrip per Part.
 *
 * Initialises each channel with default fader at 0 dB, centre pan,
 * no insert processing, and no group assignment.
 */
[[nodiscard]] MixGraph wf_create_mix_graph(
    MixGraphId id,
    const std::vector<PartId>& part_ids);

// =============================================================================
// Group Bus (§12.1: create_group_bus, assign_channel_to_group)
// =============================================================================

/**
 * @brief Create a group bus and add it to the graph.
 */
[[nodiscard]] Result<void> wf_create_group_bus(
    MixGraph& graph,
    GroupBusId id,
    const std::string& name,
    const std::vector<ChannelStripId>& member_channels);

/**
 * @brief Assign a channel to a group bus.
 */
[[nodiscard]] Result<void> wf_assign_channel_to_group(
    MixGraph& graph,
    ChannelStripId channel_id,
    GroupBusId group_id);

// =============================================================================
// Aux Bus (§12.1: create_aux_bus, set_channel_send)
// =============================================================================

/**
 * @brief Create an auxiliary bus with a name and empty effect chain.
 */
[[nodiscard]] Result<void> wf_create_aux_bus(
    MixGraph& graph,
    AuxBusId id,
    const std::string& name);

/**
 * @brief Set a channel's send level to an aux bus.
 */
[[nodiscard]] Result<void> wf_set_channel_send(
    MixGraph& graph,
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
[[nodiscard]] Result<void> wf_add_channel_effect(
    MixGraph& graph,
    ChannelStripId channel_id,
    MixEffect effect);

/**
 * @brief Add a mix effect to a group bus's insert chain.
 */
[[nodiscard]] Result<void> wf_add_bus_effect(
    MixGraph& graph,
    GroupBusId bus_id,
    MixEffect effect);

/**
 * @brief Add a mix effect to an aux bus's effect chain.
 */
[[nodiscard]] Result<void> wf_add_aux_effect(
    MixGraph& graph,
    AuxBusId aux_id,
    MixEffect effect);

/**
 * @brief Add a mix effect to the master bus chain.
 */
void wf_add_master_effect(
    MixGraph& graph,
    MixEffect effect);

// =============================================================================
// Level and Spatial (§12.1: set_channel_level, set_channel_pan, etc.)
// =============================================================================

/**
 * @brief Set a channel's fader level (absolute dB).
 */
[[nodiscard]] Result<void> wf_set_channel_level(
    MixGraph& graph,
    ChannelStripId channel_id,
    float level_db);

/**
 * @brief Set a channel's fader level relative to another channel.
 */
[[nodiscard]] Result<void> wf_set_channel_relative_level(
    MixGraph& graph,
    ChannelStripId channel_id,
    RelativeLevel relative);

/**
 * @brief Set a channel's spatial position.
 */
[[nodiscard]] Result<void> wf_set_channel_spatial(
    MixGraph& graph,
    ChannelStripId channel_id,
    SpatialPosition spatial);

/**
 * @brief Set a channel's pan position (-1.0 to +1.0).
 */
[[nodiscard]] Result<void> wf_set_channel_pan(
    MixGraph& graph,
    ChannelStripId channel_id,
    float pan);

// =============================================================================
// Intent (§12.1: set_channel_intent, set_group_intent)
// =============================================================================

/**
 * @brief Set the mixing intent for a channel.
 */
[[nodiscard]] Result<void> wf_set_channel_intent(
    MixGraph& graph,
    ChannelStripId channel_id,
    ChannelIntent intent);

/**
 * @brief Set the mixing intent for a group bus.
 */
[[nodiscard]] Result<void> wf_set_group_intent(
    MixGraph& graph,
    GroupBusId group_id,
    GroupIntent intent);

// =============================================================================
// Loudness and Output (§12.1: set_loudness_target, set_output_format)
// =============================================================================

/**
 * @brief Set the master bus loudness target.
 */
void wf_set_loudness_target(
    MixGraph& graph,
    LoudnessTarget target);

/**
 * @brief Set the output format (stereo, surround, etc.)
 */
void wf_set_output_format(
    MixGraph& graph,
    OutputFormat format);

// =============================================================================
// Automation (§12.1: add_mix_automation)
// =============================================================================

/**
 * @brief Add parameter automation to the mix graph.
 */
void wf_add_automation(
    MixGraph& graph,
    MixAutomation automation);

// =============================================================================
// Reference Profiles (§12.1: create_reference_profile, compare_to_reference)
// =============================================================================

/**
 * @brief Add a reference profile to the graph.
 */
void wf_add_reference_profile(
    MixGraph& graph,
    ReferenceProfile profile);

/**
 * @brief Compare current mix parameters against a reference profile.
 *
 * Performs static comparison of configured values (loudness target,
 * spectral shape from EQ curves) against the reference. Runtime
 * audio analysis is deferred to the render engine.
 */
[[nodiscard]] Result<ReferenceComparison> wf_compare_to_reference(
    const MixGraph& graph,
    ReferenceProfileId ref_id);

// =============================================================================
// Orchestral Seating Templates (§6.4)
// =============================================================================

enum class SeatingTemplate : std::uint8_t {
    American,       // Stokowski
    European        // Traditional
};

/**
 * @brief Apply an orchestral seating template to channel spatial positions.
 *
 * Maps instrument families to pan/depth positions based on the selected
 * arrangement. Requires channels to have ChannelIntent with frequency
 * allocation data for mapping.
 */
void wf_apply_seating_template(
    MixGraph& graph,
    SeatingTemplate seating);

// =============================================================================
// Validation (§12.1: validate_mix)
// =============================================================================

/**
 * @brief Run full validation on the mix graph. Thin wrapper over MIVD001A.
 */
[[nodiscard]] std::vector<Diagnostic> wf_validate(const MixGraph& graph);

}  // namespace Sunny::Core
