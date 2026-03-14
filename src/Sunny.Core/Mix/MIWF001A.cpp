/**
 * @file MIWF001A.cpp
 * @brief Mix IR workflow functions — implementation
 *
 * Component: MIWF001A
 * Domain: MI (Mix IR) | Category: WF (Workflow)
 *
 * Tests: TSMI005A
 */

#include "MIWF001A.h"

#include <algorithm>
#include <cmath>

namespace Sunny::Core {

namespace {

ErrorCode not_found() {
    return static_cast<ErrorCode>(MixError::NotFound);
}

ErrorCode invalid_param() {
    return static_cast<ErrorCode>(MixError::InvalidParameter);
}

ErrorCode duplicate_id() {
    return static_cast<ErrorCode>(MixError::DuplicateId);
}

// Locate a channel by its ChannelStripId; returns nullptr if absent
ChannelStrip* find_channel(MixGraph& graph, ChannelStripId id) {
    for (auto& ch : graph.channels)
        if (ch.id == id) return &ch;
    return nullptr;
}

GroupBus* find_group(MixGraph& graph, GroupBusId id) {
    for (auto& g : graph.group_buses)
        if (g.id == id) return &g;
    return nullptr;
}

AuxBus* find_aux(MixGraph& graph, AuxBusId id) {
    for (auto& a : graph.aux_buses)
        if (a.id == id) return &a;
    return nullptr;
}

}  // anonymous namespace

// =============================================================================
// Graph Construction
// =============================================================================

MixGraph wf_create_mix_graph(
    MixGraphId id,
    const std::vector<PartId>& part_ids
) {
    MixGraph graph;
    graph.id = id;

    std::uint64_t channel_id = 1;
    for (const auto& pid : part_ids) {
        ChannelStrip ch;
        ch.id = ChannelStripId{channel_id++};
        ch.part_id = pid;
        graph.channels.push_back(std::move(ch));
    }

    return graph;
}

// =============================================================================
// Group Bus
// =============================================================================

Result<void> wf_create_group_bus(
    MixGraph& graph,
    GroupBusId id,
    const std::string& name,
    const std::vector<ChannelStripId>& member_channels
) {
    // Check for duplicate group ID
    for (const auto& g : graph.group_buses)
        if (g.id == id) return std::unexpected(duplicate_id());

    // Verify all member channels exist
    for (const auto& mid : member_channels) {
        if (!find_channel(graph, mid))
            return std::unexpected(not_found());
    }

    GroupBus bus;
    bus.id = id;
    bus.name = name;
    bus.member_channels = member_channels;
    graph.group_buses.push_back(std::move(bus));

    // Update channel group assignments
    for (const auto& mid : member_channels) {
        auto* ch = find_channel(graph, mid);
        if (ch) ch->group_assignment = id;
    }

    return {};
}

Result<void> wf_assign_channel_to_group(
    MixGraph& graph,
    ChannelStripId channel_id,
    GroupBusId group_id
) {
    auto* ch = find_channel(graph, channel_id);
    if (!ch) return std::unexpected(not_found());

    auto* grp = find_group(graph, group_id);
    if (!grp) return std::unexpected(not_found());

    // Remove from previous group if assigned
    if (ch->group_assignment.has_value()) {
        auto* prev = find_group(graph, *ch->group_assignment);
        if (prev) {
            auto& mc = prev->member_channels;
            mc.erase(std::remove(mc.begin(), mc.end(), channel_id), mc.end());
        }
    }

    ch->group_assignment = group_id;
    grp->member_channels.push_back(channel_id);

    return {};
}

// =============================================================================
// Aux Bus
// =============================================================================

Result<void> wf_create_aux_bus(
    MixGraph& graph,
    AuxBusId id,
    const std::string& name
) {
    for (const auto& a : graph.aux_buses)
        if (a.id == id) return std::unexpected(duplicate_id());

    AuxBus bus;
    bus.id = id;
    bus.name = name;
    graph.aux_buses.push_back(std::move(bus));

    return {};
}

Result<void> wf_set_channel_send(
    MixGraph& graph,
    ChannelStripId channel_id,
    AuxBusId aux_id,
    float level_db,
    bool pre_fader
) {
    auto* ch = find_channel(graph, channel_id);
    if (!ch) return std::unexpected(not_found());

    if (!find_aux(graph, aux_id))
        return std::unexpected(not_found());

    // Update existing send or add new one
    for (auto& s : ch->sends) {
        if (s.aux_bus_id == aux_id) {
            s.level_db = level_db;
            s.pre_fader = pre_fader;
            s.enabled = true;
            return {};
        }
    }

    ch->sends.push_back({aux_id, level_db, pre_fader, true});
    return {};
}

// =============================================================================
// Effect Chain
// =============================================================================

Result<void> wf_add_channel_effect(
    MixGraph& graph,
    ChannelStripId channel_id,
    MixEffect effect
) {
    auto* ch = find_channel(graph, channel_id);
    if (!ch) return std::unexpected(not_found());
    ch->insert_chain.effects.push_back(std::move(effect));
    return {};
}

Result<void> wf_add_bus_effect(
    MixGraph& graph,
    GroupBusId bus_id,
    MixEffect effect
) {
    auto* g = find_group(graph, bus_id);
    if (!g) return std::unexpected(not_found());
    g->insert_chain.effects.push_back(std::move(effect));
    return {};
}

Result<void> wf_add_aux_effect(
    MixGraph& graph,
    AuxBusId aux_id,
    MixEffect effect
) {
    auto* a = find_aux(graph, aux_id);
    if (!a) return std::unexpected(not_found());
    a->effect_chain.effects.push_back(std::move(effect));
    return {};
}

void wf_add_master_effect(MixGraph& graph, MixEffect effect) {
    graph.master_bus.insert_chain.effects.push_back(std::move(effect));
}

// =============================================================================
// Level and Spatial
// =============================================================================

Result<void> wf_set_channel_level(
    MixGraph& graph,
    ChannelStripId channel_id,
    float level_db
) {
    auto* ch = find_channel(graph, channel_id);
    if (!ch) return std::unexpected(not_found());
    ch->fader.level_db = level_db;
    return {};
}

Result<void> wf_set_channel_relative_level(
    MixGraph& graph,
    ChannelStripId channel_id,
    RelativeLevel relative
) {
    auto* ch = find_channel(graph, channel_id);
    if (!ch) return std::unexpected(not_found());
    ch->fader.relative_level = relative;
    return {};
}

Result<void> wf_set_channel_spatial(
    MixGraph& graph,
    ChannelStripId channel_id,
    SpatialPosition spatial
) {
    auto* ch = find_channel(graph, channel_id);
    if (!ch) return std::unexpected(not_found());

    // Validate bounds
    if (spatial.pan < -1.0f || spatial.pan > 1.0f)
        return std::unexpected(invalid_param());
    if (spatial.depth < 0.0f || spatial.depth > 1.0f)
        return std::unexpected(invalid_param());
    if (spatial.elevation < -1.0f || spatial.elevation > 1.0f)
        return std::unexpected(invalid_param());

    ch->spatial = spatial;
    return {};
}

Result<void> wf_set_channel_pan(
    MixGraph& graph,
    ChannelStripId channel_id,
    float pan
) {
    if (pan < -1.0f || pan > 1.0f)
        return std::unexpected(invalid_param());

    auto* ch = find_channel(graph, channel_id);
    if (!ch) return std::unexpected(not_found());
    ch->spatial.pan = pan;
    return {};
}

// =============================================================================
// Intent
// =============================================================================

Result<void> wf_set_channel_intent(
    MixGraph& graph,
    ChannelStripId channel_id,
    ChannelIntent intent
) {
    auto* ch = find_channel(graph, channel_id);
    if (!ch) return std::unexpected(not_found());
    ch->intent = std::move(intent);
    return {};
}

Result<void> wf_set_group_intent(
    MixGraph& graph,
    GroupBusId group_id,
    GroupIntent intent
) {
    auto* g = find_group(graph, group_id);
    if (!g) return std::unexpected(not_found());
    g->intent = std::move(intent);
    return {};
}

// =============================================================================
// Loudness and Output
// =============================================================================

void wf_set_loudness_target(MixGraph& graph, LoudnessTarget target) {
    graph.master_bus.target_loudness = target;
}

void wf_set_output_format(MixGraph& graph, OutputFormat format) {
    graph.output_format = format;
    graph.master_bus.output_format = format;
}

// =============================================================================
// Automation
// =============================================================================

void wf_add_automation(MixGraph& graph, MixAutomation automation) {
    graph.automation.push_back(std::move(automation));
}

// =============================================================================
// Reference Profiles
// =============================================================================

void wf_add_reference_profile(MixGraph& graph, ReferenceProfile profile) {
    graph.reference_profiles.push_back(std::move(profile));
}

Result<ReferenceComparison> wf_compare_to_reference(
    const MixGraph& graph,
    ReferenceProfileId ref_id
) {
    const ReferenceProfile* ref = nullptr;
    for (const auto& r : graph.reference_profiles) {
        if (r.id == ref_id) { ref = &r; break; }
    }
    if (!ref) return std::unexpected(not_found());

    ReferenceComparison comparison;

    // Loudness comparison
    if (graph.master_bus.target_loudness) {
        comparison.loudness_difference =
            graph.master_bus.target_loudness->integrated_lufs -
            ref->loudness_profile.integrated;
    }

    // Width comparison (from spatial profiles)
    comparison.width_difference = 0.0f;  // Requires runtime analysis

    // Dynamic range comparison
    if (graph.master_bus.target_loudness &&
        graph.master_bus.target_loudness->loudness_range_lu) {
        comparison.dynamic_range_difference =
            *graph.master_bus.target_loudness->loudness_range_lu -
            ref->dynamic_profile.loudness_range;
    }

    // Spectral deviation from tonal balance curve — static approximation
    comparison.spectral_deviation = ref->tonal_balance_curve;

    return comparison;
}

// =============================================================================
// Orchestral Seating Templates
// =============================================================================

void wf_apply_seating_template(MixGraph& graph, SeatingTemplate seating) {
    // Apply heuristic spatial positions based on channel index.
    // A production implementation would map instrument families from
    // the ChannelIntent's frequency allocation. This provides
    // even distribution as a starting point.

    if (graph.channels.empty()) return;

    auto n = graph.channels.size();
    for (std::size_t i = 0; i < n; ++i) {
        auto& ch = graph.channels[i];
        float t = (n > 1)
            ? static_cast<float>(i) / static_cast<float>(n - 1)
            : 0.5f;

        if (seating == SeatingTemplate::American) {
            // Spread left-to-right, with graduated depth
            ch.spatial.pan = -0.8f + t * 1.6f;
            ch.spatial.depth = 0.2f + t * 0.3f;
        } else {
            // European: wider spread, second violins on the right
            ch.spatial.pan = -0.9f + t * 1.8f;
            ch.spatial.depth = 0.2f + t * 0.4f;
        }
    }
}

// =============================================================================
// Validation
// =============================================================================

std::vector<Diagnostic> wf_validate(const MixGraph& graph) {
    return validate_mix(graph);
}

}  // namespace Sunny::Core
