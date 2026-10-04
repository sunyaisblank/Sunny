/**
 * @file workflows.cpp
 * @brief Mix IR workflow functions — implementation
 *
 *
 */

#include <algorithm>
#include <charconv>
#include <cmath>
#include <deque>
#include <limits>
#include <map>
#include <sunny/core/mix/workflows.hpp>
#include <sunny/core/timbre/workflows.hpp>
#include <unordered_set>

namespace sunny::core {

namespace {

ErrorCode not_found() {
    return ErrorCode::MixNotFound;
}

ErrorCode invalid_param() {
    return ErrorCode::MixInvalidParameter;
}

ErrorCode duplicate_id() {
    return ErrorCode::MixDuplicateId;
}

ErrorCode inconsistent_routing_membership() {
    return ErrorCode::InconsistentRoutingMembership;
}

struct FaderNodeKey {
    FaderTargetType type = FaderTargetType::Channel;
    std::uint64_t id = 0;

    constexpr auto operator<=>(const FaderNodeKey&) const noexcept = default;
};

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

std::vector<std::string> split_parameter_path(const std::string& path) {
    std::vector<std::string> segments;
    std::string current;
    for (char character : path) {
        if (character == '.') {
            if (!current.empty()) {
                segments.push_back(std::move(current));
                current.clear();
            }
        } else if (character == '[') {
            if (!current.empty()) {
                segments.push_back(std::move(current));
                current.clear();
            }
            current.push_back(character);
        } else if (character == ']') {
            current.push_back(character);
            segments.push_back(std::move(current));
            current.clear();
        } else {
            current.push_back(character);
        }
    }
    if (!current.empty()) segments.push_back(std::move(current));
    return segments;
}

std::optional<std::size_t> parameter_index(const std::string& segment) {
    if (segment.size() < 3 || segment.front() != '[' || segment.back() != ']') return std::nullopt;
    std::size_t index = 0;
    const auto [end, error] =
        std::from_chars(segment.data() + 1, segment.data() + segment.size() - 1, index);
    if (error != std::errc{} || end != segment.data() + segment.size() - 1) return std::nullopt;
    return index;
}

/// Rejoin split segments; a path is canonical only if it survives the round trip.
std::string canonical_parameter_path(const std::vector<std::string>& segments) {
    std::string path;
    for (const auto& segment : segments) {
        if (!segment.empty() && segment.front() == '[') {
            if (segment.size() > 3 && segment[1] == '0') return {};
            path += segment;
        } else {
            if (!path.empty()) path += '.';
            path += segment;
        }
    }
    return path;
}

bool leaf(const std::vector<std::string>& segments, std::size_t position, const char* name) {
    return position + 1 == segments.size() && segments[position] == name;
}

Result<float> resolve_sidechain_filter(const MixSidechainConfig& sidechain,
                                       const std::vector<std::string>& segments,
                                       std::size_t position) {
    if (position >= segments.size()) return std::unexpected(invalid_param());
    if (leaf(segments, position, "source")) return static_cast<float>(sidechain.source);
    if (segments[position] != "filter" || !sidechain.filter || position + 1 >= segments.size())
        return std::unexpected(invalid_param());
    const auto& filter = *sidechain.filter;
    if (leaf(segments, position + 1, "filter_type")) return static_cast<float>(filter.filter_type);
    if (leaf(segments, position + 1, "frequency")) return filter.frequency;
    if (leaf(segments, position + 1, "q")) return filter.q;
    return std::unexpected(invalid_param());
}

Result<float> resolve_compressor_parameter(const MixCompressor& compressor,
                                           const std::vector<std::string>& segments,
                                           std::size_t position) {
    if (position >= segments.size()) return std::unexpected(invalid_param());
    if (leaf(segments, position, "threshold")) return compressor.threshold;
    if (leaf(segments, position, "ratio")) return compressor.ratio;
    if (leaf(segments, position, "attack")) return compressor.attack;
    if (leaf(segments, position, "release")) return compressor.release;
    if (leaf(segments, position, "knee")) return compressor.knee;
    if (leaf(segments, position, "makeup_gain")) return compressor.makeup_gain;
    if (leaf(segments, position, "detection")) return static_cast<float>(compressor.detection);
    if (leaf(segments, position, "topology")) return static_cast<float>(compressor.topology);
    if (leaf(segments, position, "stereo_link")) return compressor.stereo_link;
    if (segments[position] == "sidechain")
        return resolve_sidechain_filter(compressor.sidechain, segments, position + 1);
    return std::unexpected(invalid_param());
}

Result<float> resolve_gate_parameter(const MixGate& gate,
                                     const std::vector<std::string>& segments,
                                     std::size_t position) {
    if (position >= segments.size()) return std::unexpected(invalid_param());
    if (leaf(segments, position, "threshold")) return gate.threshold;
    if (leaf(segments, position, "ratio")) return gate.ratio;
    if (leaf(segments, position, "attack")) return gate.attack;
    if (leaf(segments, position, "hold")) return gate.hold;
    if (leaf(segments, position, "release")) return gate.release;
    if (leaf(segments, position, "range")) return gate.range;
    if (segments[position] == "sidechain")
        return resolve_sidechain_filter(gate.sidechain, segments, position + 1);
    return std::unexpected(invalid_param());
}

void append_parameter_path(std::vector<std::string>& paths,
                           const std::string& prefix,
                           const char* leaf_name) {
    paths.push_back(prefix.empty() ? leaf_name : prefix + "." + leaf_name);
}

void append_sidechain_paths(std::vector<std::string>& paths,
                            const MixSidechainConfig& sidechain,
                            const std::string& prefix) {
    append_parameter_path(paths, prefix, "source");
    if (!sidechain.filter) return;
    append_parameter_path(paths, prefix + ".filter", "filter_type");
    append_parameter_path(paths, prefix + ".filter", "frequency");
    append_parameter_path(paths, prefix + ".filter", "q");
}

void append_compressor_paths(std::vector<std::string>& paths,
                             const MixCompressor& compressor,
                             const std::string& prefix) {
    for (const auto* name : {"threshold",
                             "ratio",
                             "attack",
                             "release",
                             "knee",
                             "makeup_gain",
                             "detection",
                             "topology",
                             "stereo_link"})
        append_parameter_path(paths, prefix, name);
    append_sidechain_paths(
        paths, compressor.sidechain, prefix.empty() ? "sidechain" : prefix + ".sidechain");
}

void append_gate_paths(std::vector<std::string>& paths,
                       const MixGate& gate,
                       const std::string& prefix) {
    for (const auto* name : {"threshold", "ratio", "attack", "hold", "release", "range"})
        append_parameter_path(paths, prefix, name);
    append_sidechain_paths(
        paths, gate.sidechain, prefix.empty() ? "sidechain" : prefix + ".sidechain");
}

void append_sidechain_non_scalar_paths(std::vector<std::string>& paths,
                                       const MixSidechainConfig& sidechain,
                                       const std::string& prefix) {
    if (sidechain.source == SidechainSourceType::ExternalChannel)
        append_parameter_path(paths, prefix, "channel_id");
    if (sidechain.source == SidechainSourceType::ExternalBus)
        append_parameter_path(paths, prefix, "bus_id");
}

Result<float> resolve_mix_effect_parameter(const MixEffect& effect,
                                           const std::vector<std::string>& segments) {
    if (segments.empty()) return std::unexpected(invalid_param());
    return std::visit(
        [&](const auto& parameters) -> Result<float> {
            using T = std::decay_t<decltype(parameters)>;
            if constexpr (std::is_same_v<T, MixEQ>) {
                if (leaf(segments, 0, "linear_phase")) return parameters.linear_phase ? 1.0f : 0.0f;
                if (leaf(segments, 0, "auto_gain")) return parameters.auto_gain ? 1.0f : 0.0f;
                if (segments.size() < 3 || segments[0] != "bands")
                    return std::unexpected(invalid_param());
                const auto index = parameter_index(segments[1]);
                if (!index || *index >= parameters.bands.size())
                    return std::unexpected(invalid_param());
                const auto& band = parameters.bands[*index];
                if (leaf(segments, 2, "frequency")) return band.frequency;
                if (leaf(segments, 2, "gain")) return band.gain;
                if (leaf(segments, 2, "q")) return band.q;
                if (leaf(segments, 2, "band_type")) return static_cast<float>(band.band_type);
                if (segments[2] == "dynamic" && band.dynamic && segments.size() == 4) {
                    if (segments[3] == "threshold") return band.dynamic->threshold;
                    if (segments[3] == "ratio") return band.dynamic->ratio;
                    if (segments[3] == "attack") return band.dynamic->attack;
                    if (segments[3] == "release") return band.dynamic->release;
                }
            } else if constexpr (std::is_same_v<T, MixCompressor>) {
                return resolve_compressor_parameter(parameters, segments, 0);
            } else if constexpr (std::is_same_v<T, MixGate>) {
                return resolve_gate_parameter(parameters, segments, 0);
            } else if constexpr (std::is_same_v<T, MixLimiter>) {
                if (leaf(segments, 0, "ceiling")) return parameters.ceiling;
                if (leaf(segments, 0, "release")) return parameters.release;
                if (leaf(segments, 0, "lookahead")) return parameters.lookahead;
                if (leaf(segments, 0, "algorithm")) return static_cast<float>(parameters.algorithm);
            } else if constexpr (std::is_same_v<T, MixMultibandDynamics>) {
                if (leaf(segments, 0, "crossover_slope"))
                    return static_cast<float>(parameters.crossover_slope);
                if (segments.size() == 2 && segments[0] == "crossover_frequencies") {
                    const auto index = parameter_index(segments[1]);
                    if (index && *index < parameters.crossover_frequencies.size())
                        return parameters.crossover_frequencies[*index];
                }
                if (segments.size() >= 3 && segments[0] == "bands") {
                    const auto index = parameter_index(segments[1]);
                    if (!index || *index >= parameters.bands.size())
                        return std::unexpected(invalid_param());
                    const auto& band = parameters.bands[*index];
                    if (leaf(segments, 2, "gain")) return band.gain;
                    if (leaf(segments, 2, "solo")) return band.solo ? 1.0f : 0.0f;
                    if (segments[2] == "compressor" && band.compressor)
                        return resolve_compressor_parameter(*band.compressor, segments, 3);
                    if (segments[2] == "expander" && band.expander)
                        return resolve_gate_parameter(*band.expander, segments, 3);
                }
            } else if constexpr (std::is_same_v<T, MixSaturation>) {
                if (leaf(segments, 0, "drive")) return parameters.drive;
                if (leaf(segments, 0, "mix")) return parameters.mix;
                if (leaf(segments, 0, "output_level")) return parameters.output_level;
                if (segments.size() == 2 && segments[0] == "algorithm") {
                    if (segments[1] == "type") return static_cast<float>(parameters.algorithm.type);
                    if (segments[1] == "tape_speed")
                        return static_cast<float>(parameters.algorithm.tape_speed);
                    if (segments[1] == "tape_bias") return parameters.algorithm.tape_bias;
                    if (segments[1] == "console_type")
                        return static_cast<float>(parameters.algorithm.console_type);
                }
            } else if constexpr (std::is_same_v<T, MixStereoProcessor>) {
                if (leaf(segments, 0, "width")) return parameters.width;
                if (leaf(segments, 0, "mid_side_balance")) return parameters.mid_side_balance;
                if (leaf(segments, 0, "mono_below") && parameters.mono_below)
                    return *parameters.mono_below;
            } else if constexpr (std::is_same_v<T, MixDelay>) {
                if (leaf(segments, 0, "tempo_synced")) return parameters.tempo_synced ? 1.0f : 0.0f;
                if (leaf(segments, 0, "delay_ms")) return parameters.delay_ms;
                if (leaf(segments, 0, "beat_division"))
                    return static_cast<float>(parameters.beat_division.to_float());
                if (leaf(segments, 0, "feedback")) return parameters.feedback;
                if (leaf(segments, 0, "stereo_mode"))
                    return static_cast<float>(parameters.stereo_mode);
                if (leaf(segments, 0, "stereo_offset")) return parameters.stereo_offset;
                if (leaf(segments, 0, "low_cut_hz")) return parameters.low_cut_hz;
                if (leaf(segments, 0, "high_cut_hz")) return parameters.high_cut_hz;
                if (leaf(segments, 0, "modulation_rate")) return parameters.modulation_rate;
                if (leaf(segments, 0, "modulation_depth")) return parameters.modulation_depth;
                if (leaf(segments, 0, "mix")) return parameters.mix;
            } else if constexpr (std::is_same_v<T, MixReverb>) {
                if (leaf(segments, 0, "algorithm")) return static_cast<float>(parameters.algorithm);
                if (leaf(segments, 0, "shimmer_pitch")) return parameters.shimmer_pitch;
                if (leaf(segments, 0, "decay_time")) return parameters.decay_time;
                if (leaf(segments, 0, "pre_delay")) return parameters.pre_delay;
                if (leaf(segments, 0, "damping")) return parameters.damping;
                if (leaf(segments, 0, "diffusion")) return parameters.diffusion;
                if (leaf(segments, 0, "size")) return parameters.size;
                if (leaf(segments, 0, "early_reflections_level"))
                    return parameters.early_reflections_level;
                if (leaf(segments, 0, "low_cut_hz")) return parameters.low_cut_hz;
                if (leaf(segments, 0, "high_cut_hz")) return parameters.high_cut_hz;
                if (leaf(segments, 0, "mix")) return parameters.mix;
            }
            return std::unexpected(invalid_param());
        },
        effect.parameters);
}

void collect_effects(MixGraph& graph, std::vector<MixEffect*>& effects) {
    for (auto& channel : graph.channels)
        for (auto& effect : channel.insert_chain.effects)
            effects.push_back(&effect);
    for (auto& group : graph.group_buses)
        for (auto& effect : group.insert_chain.effects)
            effects.push_back(&effect);
    for (auto& aux : graph.aux_buses)
        for (auto& effect : aux.effect_chain.effects)
            effects.push_back(&effect);
    for (auto& effect : graph.master_bus.insert_chain.effects)
        effects.push_back(&effect);
}

} // anonymous namespace

// =============================================================================
// Graph Construction
// =============================================================================

MixGraph create_mix_graph(MixGraphId id, const std::vector<PartId>& part_ids) {
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

Result<void> create_group_bus(MixGraph& graph,
                              GroupBusId id,
                              const std::string& name,
                              const std::vector<ChannelStripId>& member_channels) {
    // Check for duplicate group ID
    for (const auto& g : graph.group_buses)
        if (g.id == id) return std::unexpected(duplicate_id());

    // Verify all member channels exist and occur exactly once before mutation.
    std::unordered_set<std::uint64_t> requested_members;
    for (const auto& mid : member_channels) {
        if (!find_channel(graph, mid)) return std::unexpected(not_found());
        if (!requested_members.insert(mid.value).second)
            return std::unexpected(inconsistent_routing_membership());
    }

    GroupBus bus;
    bus.id = id;
    bus.name = name;
    bus.member_channels = member_channels;
    graph.group_buses.push_back(std::move(bus));

    // A channel has exactly one mirrored group edge. Remove every stale reverse
    // edge rather than trusting a potentially inconsistent prior assignment.
    for (const auto& mid : member_channels) {
        for (auto& existing_group : graph.group_buses) {
            auto& existing_members = existing_group.member_channels;
            existing_members.erase(
                std::remove(existing_members.begin(), existing_members.end(), mid),
                existing_members.end());
        }

        auto* ch = find_channel(graph, mid);
        if (ch) ch->group_assignment = id;
    }

    // The cleanup above also visits the newly appended group, so restore each
    // requested edge once, in caller-specified order.
    auto* created = find_group(graph, id);
    if (created) created->member_channels = member_channels;

    return {};
}

Result<void>
assign_channel_to_group(MixGraph& graph, ChannelStripId channel_id, GroupBusId group_id) {
    auto* ch = find_channel(graph, channel_id);
    if (!ch) return std::unexpected(not_found());

    auto* grp = find_group(graph, group_id);
    if (!grp) return std::unexpected(not_found());

    // Remove all prior reverse edges, including duplicates and stale edges that
    // disagree with the channel's declared assignment.
    for (auto& existing_group : graph.group_buses) {
        auto& members = existing_group.member_channels;
        members.erase(std::remove(members.begin(), members.end(), channel_id), members.end());
    }

    ch->group_assignment = group_id;
    grp->member_channels.push_back(channel_id);

    return {};
}

Result<void>
assign_group_to_group(MixGraph& graph, GroupBusId child_group_id, GroupBusId parent_group_id) {
    if (child_group_id == parent_group_id) return std::unexpected(ErrorCode::SignalFlowCycle);
    if (!find_group(graph, child_group_id) || !find_group(graph, parent_group_id))
        return std::unexpected(not_found());

    // Validate a complete candidate before committing so cycle/depth failure
    // cannot leave one half of the mirrored edge installed.
    auto candidate = graph;
    for (auto& group : candidate.group_buses) {
        auto& children = group.member_groups;
        children.erase(std::remove(children.begin(), children.end(), child_group_id),
                       children.end());
    }
    auto* child = find_group(candidate, child_group_id);
    auto* parent = find_group(candidate, parent_group_id);
    if (!child || !parent) return std::unexpected(not_found());
    child->output.type = GroupOutputType::Group;
    child->output.parent_group_id = parent_group_id;
    parent->member_groups.push_back(child_group_id);

    const auto diagnostics = validate_mix(candidate);
    const auto error = std::find_if(diagnostics.begin(), diagnostics.end(), [](const auto& d) {
        return d.severity == ValidationSeverity::Error;
    });
    if (error != diagnostics.end()) return std::unexpected(error->error_code);

    graph = std::move(candidate);
    return {};
}

Result<void> route_group_to_master(MixGraph& graph, GroupBusId group_id) {
    if (!find_group(graph, group_id)) return std::unexpected(not_found());

    auto candidate = graph;
    for (auto& group : candidate.group_buses) {
        auto& children = group.member_groups;
        children.erase(std::remove(children.begin(), children.end(), group_id), children.end());
    }
    auto* routed = find_group(candidate, group_id);
    if (!routed) return std::unexpected(not_found());
    routed->output = GroupOutput{};

    const auto diagnostics = validate_mix(candidate);
    const auto error = std::find_if(diagnostics.begin(), diagnostics.end(), [](const auto& d) {
        return d.severity == ValidationSeverity::Error;
    });
    if (error != diagnostics.end()) return std::unexpected(error->error_code);

    graph = std::move(candidate);
    return {};
}

// =============================================================================
// Aux Bus
// =============================================================================

Result<void> create_aux_bus(MixGraph& graph, AuxBusId id, const std::string& name) {
    for (const auto& a : graph.aux_buses)
        if (a.id == id) return std::unexpected(duplicate_id());

    AuxBus bus;
    bus.id = id;
    bus.name = name;
    graph.aux_buses.push_back(std::move(bus));

    return {};
}

Result<void> set_channel_send(
    MixGraph& graph, ChannelStripId channel_id, AuxBusId aux_id, float level_db, bool pre_fader) {
    if (!std::isfinite(level_db) || level_db > MIX_LEVEL_CEILING_DB)
        return std::unexpected(invalid_param());
    auto* ch = find_channel(graph, channel_id);
    if (!ch) return std::unexpected(not_found());

    if (!find_aux(graph, aux_id)) return std::unexpected(not_found());

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

namespace {

bool valid_mix_candidate(const MixGraph& graph) {
    return std::ranges::none_of(validate_mix(graph), [](const Diagnostic& diagnostic) {
        return diagnostic.severity == ValidationSeverity::Error;
    });
}

struct MixChainLocation {
    MixEffectChain* chain;
    std::string path;
};

std::vector<MixChainLocation> mix_chains(MixGraph& graph) {
    std::vector<MixChainLocation> result;
    for (auto& channel : graph.channels)
        result.push_back({&channel.insert_chain,
                          "channels[" + std::to_string(channel.part_id.value) + "].insert_chain"});
    for (auto& group : graph.group_buses)
        result.push_back({&group.insert_chain,
                          "group_buses[" + std::to_string(group.id.value) + "].insert_chain"});
    for (auto& aux : graph.aux_buses)
        result.push_back(
            {&aux.effect_chain, "aux_buses[" + std::to_string(aux.id.value) + "].effect_chain"});
    result.push_back({&graph.master_bus.insert_chain, "master_bus.insert_chain"});
    return result;
}

Result<void>
change_mix_chain(MixGraph& graph, const std::string& path, const std::vector<MixEffectId>& order) {
    MixGraph candidate = graph;
    for (auto& location : mix_chains(candidate)) {
        if (location.path != path) continue;
        const auto before = location.chain->effects;
        if (order.size() > before.size()) return std::unexpected(invalid_param());
        std::vector<MixEffect> after;
        std::unordered_set<std::uint64_t> seen;
        for (const auto id : order) {
            const auto found = std::ranges::find(before, id, &MixEffect::id);
            if (found == before.end() || !seen.insert(id.value).second)
                return std::unexpected(invalid_param());
            after.push_back(*found);
        }
        // Position-addressed effect lanes retain the same effect identity. A
        // lane naming a removed effect is a user decision, never a retargeting.
        for (auto& lane : candidate.automation) {
            const auto relocate = [&](const std::string& prefix) -> bool {
                for (std::size_t i = 0; i < before.size(); ++i) {
                    const auto old_prefix = prefix + ".effects[" + std::to_string(i) + "].";
                    if (!lane.target.starts_with(old_prefix)) continue;
                    const auto found = std::ranges::find(after, before[i].id, &MixEffect::id);
                    if (found == after.end()) return false;
                    lane.target = prefix + ".effects[" + std::to_string(found - after.begin()) +
                                  "]." + lane.target.substr(old_prefix.size());
                    break;
                }
                return true;
            };
            if (!relocate(path)) return std::unexpected(ErrorCode::InvalidPath);
            if (path.starts_with("aux_buses[") &&
                !relocate("aux_sends" + path.substr(std::string{"aux_buses"}.size())))
                return std::unexpected(ErrorCode::InvalidPath);
        }
        for (const auto& channel : candidate.channels) {
            if (!channel.intent) continue;
            for (const auto& rationale : channel.intent->processing_rationale)
                if (std::ranges::find(before, rationale.effect_id, &MixEffect::id) !=
                        before.end() &&
                    !seen.contains(rationale.effect_id.value))
                    return std::unexpected(ErrorCode::InvalidPath);
        }
        location.chain->effects = std::move(after);
        if (!valid_mix_candidate(candidate)) return std::unexpected(invalid_param());
        graph = std::move(candidate);
        return {};
    }
    return std::unexpected(not_found());
}

} // namespace

Result<void> add_channel_effect(MixGraph& graph, ChannelStripId channel_id, MixEffect effect) {
    MixGraph candidate = graph;
    auto* ch = find_channel(candidate, channel_id);
    if (!ch) return std::unexpected(not_found());
    ch->insert_chain.effects.push_back(std::move(effect));
    if (!valid_mix_candidate(candidate)) return std::unexpected(invalid_param());
    graph = std::move(candidate);
    return {};
}

Result<void> add_bus_effect(MixGraph& graph, GroupBusId bus_id, MixEffect effect) {
    MixGraph candidate = graph;
    auto* g = find_group(candidate, bus_id);
    if (!g) return std::unexpected(not_found());
    g->insert_chain.effects.push_back(std::move(effect));
    if (!valid_mix_candidate(candidate)) return std::unexpected(invalid_param());
    graph = std::move(candidate);
    return {};
}

Result<void> add_aux_effect(MixGraph& graph, AuxBusId aux_id, MixEffect effect) {
    MixGraph candidate = graph;
    auto* a = find_aux(candidate, aux_id);
    if (!a) return std::unexpected(not_found());
    a->effect_chain.effects.push_back(std::move(effect));
    if (!valid_mix_candidate(candidate)) return std::unexpected(invalid_param());
    graph = std::move(candidate);
    return {};
}

Result<void> add_master_effect(MixGraph& graph, MixEffect effect) {
    MixGraph candidate = graph;
    candidate.master_bus.insert_chain.effects.push_back(std::move(effect));
    if (!valid_mix_candidate(candidate)) return std::unexpected(invalid_param());
    graph = std::move(candidate);
    return {};
}

Result<void> replace_mix_effect(MixGraph& graph,
                                MixEffectId effect_id,
                                MixEffectParameters parameters,
                                bool enabled) {
    MixGraph candidate = graph;
    for (auto& location : mix_chains(candidate)) {
        const auto found = std::ranges::find(location.chain->effects, effect_id, &MixEffect::id);
        if (found == location.chain->effects.end()) continue;
        found->parameters = std::move(parameters);
        found->enabled = enabled;
        if (!valid_mix_candidate(candidate)) return std::unexpected(invalid_param());
        graph = std::move(candidate);
        return {};
    }
    return std::unexpected(not_found());
}

Result<void> remove_mix_effect(MixGraph& graph, MixEffectId effect_id) {
    for (const auto& location : mix_chains(graph)) {
        if (std::ranges::find(location.chain->effects, effect_id, &MixEffect::id) ==
            location.chain->effects.end())
            continue;
        std::vector<MixEffectId> order;
        for (const auto& effect : location.chain->effects)
            if (effect.id != effect_id) order.push_back(effect.id);
        return change_mix_chain(graph, location.path, order);
    }
    return std::unexpected(not_found());
}

Result<void> reorder_mix_effects(MixGraph& graph,
                                 const std::string& chain_path,
                                 const std::vector<MixEffectId>& order) {
    for (const auto& location : mix_chains(graph))
        if (location.path == chain_path && order.size() != location.chain->effects.size())
            return std::unexpected(invalid_param());
    return change_mix_chain(graph, chain_path, order);
}

Result<void> remove_mix_automation(MixGraph& graph, std::size_t index) {
    if (index >= graph.automation.size()) return std::unexpected(not_found());
    graph.automation.erase(graph.automation.begin() + static_cast<std::ptrdiff_t>(index));
    return {};
}

Result<void> remove_mix_parameter_mapping(MixGraph& graph,
                                          MixEffectId effect_id,
                                          const std::string& source_path) {
    for (const auto& location : mix_chains(graph)) {
        const auto found = std::ranges::find(location.chain->effects, effect_id, &MixEffect::id);
        if (found != location.chain->effects.end()) {
            if (found->parameter_map.erase(source_path) == 0) return std::unexpected(not_found());
            return {};
        }
    }
    return std::unexpected(not_found());
}

Result<float> get_mix_effect_parameter(const MixEffect& effect, const std::string& path) {
    return resolve_mix_effect_parameter(effect, split_parameter_path(path));
}

std::vector<std::string> mix_effect_parameter_paths(const MixEffect& effect) {
    std::vector<std::string> paths;
    std::visit(
        [&](const auto& parameters) {
            using T = std::decay_t<decltype(parameters)>;
            if constexpr (std::is_same_v<T, MixEQ>) {
                paths.emplace_back("linear_phase");
                paths.emplace_back("auto_gain");
                for (std::size_t index = 0; index < parameters.bands.size(); ++index) {
                    const auto prefix = "bands[" + std::to_string(index) + "]";
                    for (const auto* name : {"frequency", "gain", "q", "band_type"})
                        append_parameter_path(paths, prefix, name);
                    if (parameters.bands[index].dynamic) {
                        for (const auto* name : {"threshold", "ratio", "attack", "release"})
                            append_parameter_path(paths, prefix + ".dynamic", name);
                    }
                }
            } else if constexpr (std::is_same_v<T, MixCompressor>) {
                append_compressor_paths(paths, parameters, "");
            } else if constexpr (std::is_same_v<T, MixGate>) {
                append_gate_paths(paths, parameters, "");
            } else if constexpr (std::is_same_v<T, MixLimiter>) {
                paths = {"ceiling", "release", "lookahead", "algorithm"};
            } else if constexpr (std::is_same_v<T, MixMultibandDynamics>) {
                paths.emplace_back("crossover_slope");
                for (std::size_t index = 0; index < parameters.crossover_frequencies.size();
                     ++index)
                    paths.push_back("crossover_frequencies[" + std::to_string(index) + "]");
                for (std::size_t index = 0; index < parameters.bands.size(); ++index) {
                    const auto prefix = "bands[" + std::to_string(index) + "]";
                    append_parameter_path(paths, prefix, "gain");
                    append_parameter_path(paths, prefix, "solo");
                    if (parameters.bands[index].compressor)
                        append_compressor_paths(
                            paths, *parameters.bands[index].compressor, prefix + ".compressor");
                    if (parameters.bands[index].expander)
                        append_gate_paths(
                            paths, *parameters.bands[index].expander, prefix + ".expander");
                }
            } else if constexpr (std::is_same_v<T, MixSaturation>) {
                paths = {"algorithm.type", "drive", "mix", "output_level"};
                if (parameters.algorithm.type == SaturationTypeTag::Tape) {
                    paths.emplace_back("algorithm.tape_speed");
                    paths.emplace_back("algorithm.tape_bias");
                } else if (parameters.algorithm.type == SaturationTypeTag::Console) {
                    paths.emplace_back("algorithm.console_type");
                }
            } else if constexpr (std::is_same_v<T, MixStereoProcessor>) {
                paths = {"width", "mid_side_balance"};
                if (parameters.mono_below) paths.emplace_back("mono_below");
            } else if constexpr (std::is_same_v<T, MixDelay>) {
                paths = {"tempo_synced",
                         "feedback",
                         "stereo_mode",
                         "stereo_offset",
                         "low_cut_hz",
                         "high_cut_hz",
                         "modulation_rate",
                         "modulation_depth",
                         "mix"};
                paths.emplace_back(parameters.tempo_synced ? "beat_division" : "delay_ms");
            } else if constexpr (std::is_same_v<T, MixReverb>) {
                paths = {"algorithm",
                         "decay_time",
                         "pre_delay",
                         "damping",
                         "diffusion",
                         "size",
                         "early_reflections_level",
                         "low_cut_hz",
                         "high_cut_hz",
                         "mix"};
                if (parameters.algorithm == MixReverbAlgorithm::Shimmer)
                    paths.emplace_back("shimmer_pitch");
            }
        },
        effect.parameters);
    return paths;
}

std::vector<std::string> mix_effect_non_scalar_paths(const MixEffect& effect) {
    std::vector<std::string> paths;
    std::visit(
        [&](const auto& parameters) {
            using T = std::decay_t<decltype(parameters)>;
            if constexpr (std::is_same_v<T, MixCompressor> || std::is_same_v<T, MixGate>) {
                append_sidechain_non_scalar_paths(paths, parameters.sidechain, "sidechain");
            } else if constexpr (std::is_same_v<T, MixMultibandDynamics>) {
                for (std::size_t index = 0; index < parameters.bands.size(); ++index) {
                    const auto prefix = "bands[" + std::to_string(index) + "]";
                    if (parameters.bands[index].compressor)
                        append_sidechain_non_scalar_paths(
                            paths,
                            parameters.bands[index].compressor->sidechain,
                            prefix + ".compressor.sidechain");
                    if (parameters.bands[index].expander)
                        append_sidechain_non_scalar_paths(
                            paths,
                            parameters.bands[index].expander->sidechain,
                            prefix + ".expander.sidechain");
                }
            } else if constexpr (std::is_same_v<T, MixSaturation>) {
                if (parameters.algorithm.type == SaturationTypeTag::Tube &&
                    !parameters.algorithm.tube_model.empty())
                    paths.emplace_back("algorithm.tube_model");
            } else if constexpr (std::is_same_v<T, MixReverb>) {
                if (parameters.algorithm == MixReverbAlgorithm::Convolution &&
                    !parameters.impulse_response.empty())
                    paths.emplace_back("impulse_response");
            }
        },
        effect.parameters);
    return paths;
}

Result<float> map_mix_device_parameter_value(float source_value,
                                             const MixDeviceParameter& mapping) {
    DeviceParameter common;
    common.parameter_name = mapping.parameter_name;
    common.range_min = mapping.range_min;
    common.range_max = mapping.range_max;
    common.curve = mapping.curve;
    common.source_min = mapping.source_min;
    common.source_max = mapping.source_max;
    common.value_property = mapping.value_property;
    auto result = map_device_parameter_value(source_value, common);
    if (!result) return std::unexpected(invalid_param());
    return *result;
}

Result<void> map_mix_effect_parameter(MixGraph& graph,
                                      MixEffectId effect_id,
                                      const std::string& source_path,
                                      MixDeviceParameter mapping) {
    if (source_path.empty() || mapping.parameter_name.empty())
        return std::unexpected(invalid_param());

    std::vector<MixEffect*> effects;
    collect_effects(graph, effects);
    MixEffect* match = nullptr;
    for (auto* effect : effects) {
        if (effect->id != effect_id) continue;
        if (match != nullptr) return std::unexpected(duplicate_id());
        match = effect;
    }
    if (match == nullptr) return std::unexpected(not_found());

    auto source = get_mix_effect_parameter(*match, source_path);
    if (!source) return std::unexpected(source.error());
    auto target = map_mix_device_parameter_value(*source, mapping);
    if (!target) return std::unexpected(target.error());
    for (const auto& [existing_path, existing] : match->parameter_map) {
        if (existing_path != source_path && existing.parameter_name == mapping.parameter_name)
            return std::unexpected(invalid_param());
    }
    match->parameter_map[source_path] = std::move(mapping);
    return {};
}

// =============================================================================
// Level and Spatial
// =============================================================================

Result<void> set_channel_level(MixGraph& graph, ChannelStripId channel_id, float level_db) {
    if (!std::isfinite(level_db) || level_db > MIX_LEVEL_CEILING_DB)
        return std::unexpected(invalid_param());
    auto* ch = find_channel(graph, channel_id);
    if (!ch) return std::unexpected(not_found());
    ch->fader.level_db = level_db;
    ch->fader.relative_level.reset();
    return {};
}

Result<void> set_channel_flags(MixGraph& graph,
                               ChannelStripId channel_id,
                               std::optional<bool> mute,
                               std::optional<bool> solo) {
    if (!mute && !solo) return std::unexpected(invalid_param());
    auto* channel = find_channel(graph, channel_id);
    if (!channel) return std::unexpected(not_found());
    if (mute) channel->mute = *mute;
    if (solo) channel->solo = *solo;
    return {};
}

Result<void> set_channel_relative_level(MixGraph& graph,
                                        ChannelStripId channel_id,
                                        const RelativeLevel& relative) {
    MixGraph candidate = graph;
    auto* ch = find_channel(candidate, channel_id);
    if (!ch) return std::unexpected(not_found());
    ch->fader.relative_level = relative;
    auto resolution = resolve_relative_levels(candidate);
    if (!resolution) return std::unexpected(resolution.error());
    graph = std::move(candidate);
    return {};
}

Result<RelativeLevelResolution> resolve_relative_levels(const MixGraph& graph) {
    std::map<FaderNodeKey, const Fader*> nodes;
    std::vector<FaderNodeKey> order;
    constexpr auto max_size = std::numeric_limits<std::size_t>::max();
    if (graph.group_buses.size() == max_size ||
        graph.channels.size() > max_size - graph.group_buses.size() - 1)
        return std::unexpected(invalid_param());
    order.reserve(graph.channels.size() + graph.group_buses.size() + 1);

    const auto add = [&](FaderNodeKey key, const Fader& fader) -> bool {
        if (!nodes.emplace(key, &fader).second) return false;
        order.push_back(key);
        return true;
    };
    for (const auto& channel : graph.channels) {
        if (!add({FaderTargetType::Channel, channel.id.value}, channel.fader))
            return std::unexpected(duplicate_id());
    }
    for (const auto& group : graph.group_buses) {
        if (!add({FaderTargetType::Group, group.id.value}, group.fader))
            return std::unexpected(duplicate_id());
    }
    if (!add({FaderTargetType::Master, 0}, graph.master_bus.fader))
        return std::unexpected(duplicate_id());

    // Each relative node has exactly one incoming dependency. Kahn's
    // algorithm keeps evaluation linear and avoids graph-controlled call
    // stack depth. Ordered construction makes equal inputs deterministic.
    std::map<FaderNodeKey, std::uint8_t> in_degree;
    std::map<FaderNodeKey, std::vector<FaderNodeKey>> dependants;
    std::map<FaderNodeKey, std::optional<float>> values;
    std::map<FaderNodeKey, FaderLevelResolutionStatus> statuses;

    for (const auto key : order) {
        const auto& fader = *nodes.at(key);
        if (!std::isfinite(fader.level_db) || fader.level_db > MIX_LEVEL_CEILING_DB)
            return std::unexpected(invalid_param());

        if (!fader.relative_level) {
            values[key] = fader.level_db;
            statuses[key] = FaderLevelResolutionStatus::Explicit;
            in_degree[key] = 0;
            continue;
        }

        const auto& relative = *fader.relative_level;
        if (!std::isfinite(relative.offset_db)) return std::unexpected(invalid_param());

        if (relative.reference.type == LevelReferenceType::MasterTarget) {
            if (!std::isfinite(relative.reference.lufs) || relative.reference.lufs > 0.0f)
                return std::unexpected(invalid_param());
            values[key] = std::nullopt;
            statuses[key] = FaderLevelResolutionStatus::RequiresLoudnessMeasurement;
            in_degree[key] = 0;
            continue;
        }

        FaderNodeKey reference;
        if (relative.reference.type == LevelReferenceType::Channel) {
            reference = {FaderTargetType::Channel, relative.reference.channel_id.value};
        } else if (relative.reference.type == LevelReferenceType::Group) {
            reference = {FaderTargetType::Group, relative.reference.group_id.value};
        } else {
            return std::unexpected(invalid_param());
        }
        if (!nodes.contains(reference)) return std::unexpected(invalid_param());
        in_degree[key] = 1;
        dependants[reference].push_back(key);
    }

    std::deque<FaderNodeKey> ready;
    for (const auto key : order)
        if (in_degree.at(key) == 0) ready.push_back(key);

    std::size_t processed = 0;
    while (!ready.empty()) {
        const auto key = ready.front();
        ready.pop_front();
        processed++;
        for (const auto dependant : dependants[key]) {
            const auto& relative = *nodes.at(dependant)->relative_level;
            if (!values.at(key)) {
                values[dependant] = std::nullopt;
                statuses[dependant] = FaderLevelResolutionStatus::BlockedByUnresolvedReference;
            } else {
                const float derived = *values.at(key) + relative.offset_db;
                if (!std::isfinite(derived) || derived > MIX_LEVEL_CEILING_DB)
                    return std::unexpected(invalid_param());
                values[dependant] = derived;
                statuses[dependant] = FaderLevelResolutionStatus::Resolved;
            }
            in_degree[dependant] = 0;
            ready.push_back(dependant);
        }
    }
    if (processed != nodes.size()) return std::unexpected(invalid_param());

    RelativeLevelResolution result;
    result.levels.reserve(order.size());
    for (const auto key : order) {
        const auto& fader = *nodes.at(key);
        FaderLevelResolution entry;
        entry.target_type = key.type;
        entry.target_id = key.id;
        entry.explicit_level_db = fader.level_db;
        entry.resolved_level_db = values.at(key);
        entry.status = statuses.at(key);
        if (fader.relative_level) {
            entry.reference = fader.relative_level->reference;
            entry.offset_db = fader.relative_level->offset_db;
            if (result.relative_levels_total == std::numeric_limits<std::uint32_t>::max())
                return std::unexpected(invalid_param());
            result.relative_levels_total++;
            if (entry.status == FaderLevelResolutionStatus::Resolved) {
                if (result.relative_levels_resolved == std::numeric_limits<std::uint32_t>::max())
                    return std::unexpected(invalid_param());
                result.relative_levels_resolved++;
            } else {
                if (result.relative_levels_unresolved == std::numeric_limits<std::uint32_t>::max())
                    return std::unexpected(invalid_param());
                result.relative_levels_unresolved++;
            }
        }
        result.levels.push_back(std::move(entry));
    }
    return result;
}

Result<void>
set_channel_spatial(MixGraph& graph, ChannelStripId channel_id, SpatialPosition spatial) {
    auto* ch = find_channel(graph, channel_id);
    if (!ch) return std::unexpected(not_found());

    // Validate bounds
    if (spatial.pan < -1.0f || spatial.pan > 1.0f) return std::unexpected(invalid_param());
    if (spatial.depth < 0.0f || spatial.depth > 1.0f) return std::unexpected(invalid_param());
    if (spatial.elevation < -1.0f || spatial.elevation > 1.0f)
        return std::unexpected(invalid_param());

    ch->spatial = spatial;
    return {};
}

Result<void> set_channel_pan(MixGraph& graph, ChannelStripId channel_id, float pan) {
    if (pan < -1.0f || pan > 1.0f) return std::unexpected(invalid_param());

    auto* ch = find_channel(graph, channel_id);
    if (!ch) return std::unexpected(not_found());
    ch->spatial.pan = pan;
    return {};
}

// =============================================================================
// Intent
// =============================================================================

Result<void> set_channel_intent(MixGraph& graph, ChannelStripId channel_id, ChannelIntent intent) {
    auto* ch = find_channel(graph, channel_id);
    if (!ch) return std::unexpected(not_found());
    ch->intent = std::move(intent);
    return {};
}

Result<void> set_group_intent(MixGraph& graph, GroupBusId group_id, GroupIntent intent) {
    auto* g = find_group(graph, group_id);
    if (!g) return std::unexpected(not_found());
    g->intent = std::move(intent);
    return {};
}

// =============================================================================
// Loudness and Output
// =============================================================================

Result<void> set_loudness_target(MixGraph& graph, LoudnessTarget target) {
    if (!is_loudness_target_valid(target)) return std::unexpected(invalid_param());
    graph.master_bus.target_loudness = target;
    return {};
}

void set_output_format(MixGraph& graph, OutputFormat format) {
    graph.output_format = format;
    graph.master_bus.output_format = format;
}

// =============================================================================
// Automation
// =============================================================================

Result<float> get_mix_automation_target(const MixGraph& graph, const std::string& path) {
    const auto segments = split_parameter_path(path);
    if (segments.empty() || canonical_parameter_path(segments) != path)
        return std::unexpected(ErrorCode::InvalidPath);

    // Resolve the owner, then the owner-relative tail. Identity-addressed
    // collections take their key from the bracket; effect chains are ordered
    // and take a position, exactly as in the §9.2 examples.
    const auto key = [&](std::size_t position) -> std::optional<std::uint64_t> {
        if (position >= segments.size()) return std::nullopt;
        const auto value = parameter_index(segments[position]);
        if (!value) return std::nullopt;
        return static_cast<std::uint64_t>(*value);
    };
    const auto unresolved = [] { return Result<float>{std::unexpected(ErrorCode::InvalidPath)}; };
    const auto spatial_axis = [&](const SpatialPosition& spatial,
                                  std::size_t position) -> Result<float> {
        if (leaf(segments, position, "pan")) return spatial.pan;
        if (leaf(segments, position, "depth")) return spatial.depth;
        if (leaf(segments, position, "elevation")) return spatial.elevation;
        if (leaf(segments, position, "width")) return spatial.width;
        return unresolved();
    };
    const auto chain_parameter = [&](const MixEffectChain& chain,
                                     std::size_t position) -> Result<float> {
        const auto index = key(position + 1);
        if (position + 3 >= segments.size() || segments[position] != "effects" || !index ||
            *index >= chain.effects.size() || segments[position + 2] != "parameters")
            return unresolved();
        const std::vector<std::string> tail(
            segments.begin() + static_cast<std::ptrdiff_t>(position + 3), segments.end());
        auto value = resolve_mix_effect_parameter(chain.effects[*index], tail);
        if (!value) return unresolved();
        return value;
    };
    const auto send_level = [&](const std::vector<AuxSendLevel>& sends,
                                std::size_t position) -> Result<float> {
        const auto aux = key(position);
        if (!aux || !leaf(segments, position + 1, "level_db")) return unresolved();
        for (const auto& send : sends)
            if (send.aux_bus_id.value == *aux) return send.level_db;
        return unresolved();
    };
    const auto strip = [&](const auto& owner, std::size_t position) -> Result<float> {
        if (position >= segments.size()) return unresolved();
        const auto& field = segments[position];
        if (field == "fader" && leaf(segments, position + 1, "level_db"))
            return owner.fader.level_db;
        if (field == "spatial") return spatial_axis(owner.spatial, position + 1);
        if (field == "sends") return send_level(owner.sends, position + 1);
        if (field == "insert_chain") return chain_parameter(owner.insert_chain, position + 1);
        return unresolved();
    };

    const auto& root = segments[0];
    if (root == "channels") {
        const auto part = key(1);
        if (!part) return unresolved();
        for (const auto& channel : graph.channels) {
            if (channel.part_id.value != *part) continue;
            if (leaf(segments, 2, "input_trim")) return channel.input_trim;
            return strip(channel, 2);
        }
        return unresolved();
    }
    if (root == "group_buses") {
        const auto id = key(1);
        if (!id) return unresolved();
        for (const auto& group : graph.group_buses)
            if (group.id.value == *id) return strip(group, 2);
        return unresolved();
    }
    if (root == "master_bus") {
        if (leaf(segments, 2, "level_db") && segments[1] == "fader")
            return graph.master_bus.fader.level_db;
        if (segments.size() > 1 && segments[1] == "insert_chain")
            return chain_parameter(graph.master_bus.insert_chain, 2);
        return unresolved();
    }
    if (root == "aux_buses" || root == "aux_sends") {
        const auto id = key(1);
        if (!id) return unresolved();
        for (const auto& aux : graph.aux_buses) {
            if (aux.id.value != *id) continue;
            if (leaf(segments, 2, "return_level")) return aux.return_level;
            if (segments.size() > 2 && segments[2] == "return_spatial")
                return spatial_axis(aux.return_spatial, 3);
            if (segments.size() > 2 && segments[2] == "effect_chain")
                return chain_parameter(aux.effect_chain, 3);
            return unresolved();
        }
        return unresolved();
    }
    return unresolved();
}

Result<void> add_automation(MixGraph& graph, MixAutomation automation) {
    if (auto valid = validate_mix_automation(graph, automation); !valid)
        return std::unexpected(valid.error());
    graph.automation.push_back(std::move(automation));
    return {};
}

// =============================================================================
// Reference Profiles
// =============================================================================

void add_reference_profile(MixGraph& graph, ReferenceProfile profile) {
    graph.reference_profiles.push_back(std::move(profile));
}

Result<ReferenceComparison> compare_to_reference(const MixGraph& graph, ReferenceProfileId ref_id) {
    const ReferenceProfile* ref = nullptr;
    for (const auto& r : graph.reference_profiles) {
        if (r.id == ref_id) {
            ref = &r;
            break;
        }
    }
    if (!ref) return std::unexpected(not_found());

    ReferenceComparison comparison;

    // Only configured intent can stand in for the mix side of a difference.
    // Loudness and loudness range have configured targets; the spectrum and
    // stereo width have no mix-side value until rendered audio is measured,
    // so those differences stay absent rather than being invented.
    if (const auto& target = graph.master_bus.target_loudness) {
        comparison.loudness_difference = target->integrated_lufs - ref->loudness_profile.integrated;
        if (target->loudness_range_lu)
            comparison.dynamic_range_difference =
                *target->loudness_range_lu - ref->dynamic_profile.loudness_range;
    }

    return comparison;
}

// =============================================================================
// Orchestral Seating Templates
// =============================================================================

SpatialPosition seating_position(SeatingTemplate seating, OrchestralSection section) {
    // Each §6.4 interval is represented by its midpoint so a section sits in
    // the middle of its seat rather than at an arbitrary edge.
    struct Seat {
        float pan;
        float depth;
    };
    const auto american = [](OrchestralSection value) -> Seat {
        switch (value) {
        case OrchestralSection::ViolinI:
            return {-0.4f, 0.3f};
        case OrchestralSection::ViolinII:
            return {0.0f, 0.3f};
        case OrchestralSection::Viola:
            return {0.35f, 0.4f};
        case OrchestralSection::Cello:
            return {0.6f, 0.4f};
        case OrchestralSection::DoubleBass:
            return {0.8f, 0.5f};
        case OrchestralSection::Flutes:
            return {-0.2f, 0.45f};
        case OrchestralSection::Oboes:
            return {0.0f, 0.45f};
        case OrchestralSection::Clarinets:
            return {0.2f, 0.45f};
        case OrchestralSection::Bassoons:
            return {0.4f, 0.5f};
        case OrchestralSection::Horns:
            return {-0.35f, 0.6f};
        case OrchestralSection::Trumpets:
            return {0.0f, 0.65f};
        case OrchestralSection::Trombones:
            return {0.35f, 0.65f};
        case OrchestralSection::Tuba:
            return {0.4f, 0.65f};
        case OrchestralSection::Timpani:
            return {0.4f, 0.75f};
        case OrchestralSection::Percussion:
            return {0.0f, 0.75f};
        case OrchestralSection::Harp:
            return {-0.7f, 0.4f};
        }
        return {0.0f, 0.0f};
    };

    Seat seat = american(section);
    if (seating == SeatingTemplate::European) {
        // §6.4 moves only two sections: Violin II to +0.2..+0.6, and the
        // cellos to the left, taken as the mirror image of their American
        // seat (-0.7..-0.5).
        if (section == OrchestralSection::ViolinII) seat.pan = 0.4f;
        if (section == OrchestralSection::Cello) seat.pan = -0.6f;
    }
    SpatialPosition position;
    position.pan = seat.pan;
    position.depth = seat.depth;
    return position;
}

Result<void> apply_seating_template(MixGraph& graph,
                                    SeatingTemplate seating,
                                    const std::vector<SeatingAssignment>& assignments) {
    std::unordered_set<std::uint64_t> seen;
    for (const auto& assignment : assignments) {
        if (static_cast<std::uint8_t>(assignment.section) > ORCHESTRAL_SECTION_MAX)
            return std::unexpected(invalid_param());
        if (!find_channel(graph, assignment.channel_id)) return std::unexpected(not_found());
        if (!seen.insert(assignment.channel_id.value).second)
            return std::unexpected(duplicate_id());
    }
    for (const auto& assignment : assignments) {
        const auto seat = seating_position(seating, assignment.section);
        auto* channel = find_channel(graph, assignment.channel_id);
        channel->spatial.pan = seat.pan;
        channel->spatial.depth = seat.depth;
    }
    return {};
}

// =============================================================================
// Validation
// =============================================================================

std::vector<Diagnostic> validate(const MixGraph& graph) {
    return validate_mix(graph);
}

} // namespace sunny::core
