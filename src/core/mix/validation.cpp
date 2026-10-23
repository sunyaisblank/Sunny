/**
 * @file validation.cpp
 * @brief Mix IR validation implementation
 *
 *
 */

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <sunny/core/mix/validation.hpp>
#include <sunny/core/mix/workflows.hpp>
#include <unordered_map>
#include <unordered_set>

namespace sunny::core {

namespace {

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

void add_diagnostic(std::vector<Diagnostic>& out,
                    ValidationSeverity sev,
                    const char* rule,
                    const std::string& msg,
                    ErrorCode code,
                    std::optional<PartId> part = std::nullopt) {
    out.push_back({sev, rule, msg, std::nullopt, part, code});
}

// -------------------------------------------------------------------------
// X0: identity uniqueness
// -------------------------------------------------------------------------

void check_unique_ids(const MixGraph& graph, std::vector<Diagnostic>& out) {
    const auto check = [&](const auto& values, const char* kind, auto id) {
        std::unordered_set<std::uint64_t> seen;
        for (const auto& value : values) {
            const auto identifier = id(value);
            if (!seen.insert(identifier).second) {
                add_diagnostic(out,
                               ValidationSeverity::Error,
                               "X0",
                               std::string("Duplicate ") + kind + " ID " +
                                   std::to_string(identifier),
                               ErrorCode::MixDuplicateId);
            }
        }
    };

    check(graph.channels, "ChannelStrip", [](const ChannelStrip& value) { return value.id.value; });
    check(graph.group_buses, "GroupBus", [](const GroupBus& value) { return value.id.value; });
    check(graph.aux_buses, "AuxBus", [](const AuxBus& value) { return value.id.value; });
    check(graph.reference_profiles, "ReferenceProfile", [](const ReferenceProfile& value) {
        return value.id.value;
    });

    std::vector<const MixEffect*> effects;
    for (const auto& channel : graph.channels)
        for (const auto& effect : channel.insert_chain.effects)
            effects.push_back(&effect);
    for (const auto& group : graph.group_buses)
        for (const auto& effect : group.insert_chain.effects)
            effects.push_back(&effect);
    for (const auto& aux : graph.aux_buses)
        for (const auto& effect : aux.effect_chain.effects)
            effects.push_back(&effect);
    for (const auto& effect : graph.master_bus.insert_chain.effects)
        effects.push_back(&effect);
    check(effects, "MixEffect", [](const MixEffect* value) { return value->id.value; });
}

// -------------------------------------------------------------------------
// X10: redundant group membership and routing references are exact mirrors
// -------------------------------------------------------------------------

void check_routing_membership_consistency(const MixGraph& graph, std::vector<Diagnostic>& out) {
    std::unordered_map<std::uint64_t, const ChannelStrip*> channels;
    std::unordered_map<std::uint64_t, const GroupBus*> groups;
    for (const auto& channel : graph.channels)
        channels[channel.id.value] = &channel;
    for (const auto& group : graph.group_buses)
        groups[group.id.value] = &group;

    const auto inconsistent = [&](std::string message) {
        add_diagnostic(out,
                       ValidationSeverity::Error,
                       "X10",
                       std::move(message),
                       ErrorCode::InconsistentRoutingMembership);
    };

    for (const auto& group : graph.group_buses) {
        std::unordered_set<std::uint64_t> channel_members;
        for (const auto member : group.member_channels) {
            const auto channel = channels.find(member.value);
            if (!channel_members.insert(member.value).second) {
                inconsistent("Group " + std::to_string(group.id.value) +
                             " contains duplicate channel member " + std::to_string(member.value));
            } else if (channel == channels.end()) {
                inconsistent("Group " + std::to_string(group.id.value) +
                             " references missing channel member " + std::to_string(member.value));
            } else if (!channel->second->group_assignment ||
                       channel->second->group_assignment->value != group.id.value) {
                inconsistent("Group " + std::to_string(group.id.value) + " lists channel " +
                             std::to_string(member.value) +
                             " but the channel does not route to that group");
            }
        }

        std::unordered_set<std::uint64_t> group_members;
        for (const auto member : group.member_groups) {
            const auto child = groups.find(member.value);
            if (!group_members.insert(member.value).second) {
                inconsistent("Group " + std::to_string(group.id.value) +
                             " contains duplicate child group " + std::to_string(member.value));
            } else if (child == groups.end()) {
                inconsistent("Group " + std::to_string(group.id.value) +
                             " references missing child group " + std::to_string(member.value));
            } else if (child->second->output.type != GroupOutputType::Group ||
                       child->second->output.parent_group_id.value != group.id.value) {
                inconsistent("Group " + std::to_string(group.id.value) + " lists child group " +
                             std::to_string(member.value) +
                             " but the child does not route to that parent");
            }
        }
    }

    for (const auto& channel : graph.channels) {
        if (!channel.group_assignment) continue;
        const auto group = groups.find(channel.group_assignment->value);
        const auto count =
            group == groups.end()
                ? std::size_t{0}
                : static_cast<std::size_t>(std::count(group->second->member_channels.begin(),
                                                      group->second->member_channels.end(),
                                                      channel.id));
        if (count != 1) {
            inconsistent("Channel " + std::to_string(channel.id.value) + " routes to group " +
                         std::to_string(channel.group_assignment->value) +
                         " but that group does not list the channel exactly once");
        }
    }

    for (const auto& child : graph.group_buses) {
        if (child.output.type != GroupOutputType::Group) continue;
        const auto parent = groups.find(child.output.parent_group_id.value);
        const auto count =
            parent == groups.end()
                ? std::size_t{0}
                : static_cast<std::size_t>(std::count(parent->second->member_groups.begin(),
                                                      parent->second->member_groups.end(),
                                                      child.id));
        if (count != 1) {
            inconsistent("Group " + std::to_string(child.id.value) + " routes to parent group " +
                         std::to_string(child.output.parent_group_id.value) +
                         " but that parent does not list the child exactly once");
        }
    }
}

// -------------------------------------------------------------------------
// X2: Signal flow DAG — acyclicity check
//
// Builds the group-to-group adjacency from member_groups and output
// routing, then runs a topological sort (Kahn's algorithm). If any
// nodes remain after the sort, a cycle exists.
// -------------------------------------------------------------------------

void check_dag_acyclicity(const MixGraph& graph, std::vector<Diagnostic>& out) {
    // Build adjacency: parent → child for member_groups; child → parent for output.Group
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> adj;
    std::unordered_map<std::uint64_t, int> in_degree;

    for (const auto& g : graph.group_buses) {
        if (in_degree.find(g.id.value) == in_degree.end()) in_degree[g.id.value] = 0;
        for (const auto& child_id : g.member_groups) {
            adj[g.id.value].push_back(child_id.value);
            in_degree[child_id.value]++;
        }
    }

    // Also check output routing: if a group routes to another group
    for (const auto& g : graph.group_buses) {
        if (g.output.type == GroupOutputType::Group) {
            auto parent = g.output.parent_group_id.value;
            adj[parent].push_back(g.id.value);
            in_degree[g.id.value]++;
        }
    }

    // Kahn's algorithm
    std::vector<std::uint64_t> queue;
    for (const auto& [node, deg] : in_degree) {
        if (deg == 0) queue.push_back(node);
    }

    std::size_t processed = 0;
    while (!queue.empty()) {
        auto node = queue.back();
        queue.pop_back();
        processed++;
        if (adj.find(node) != adj.end()) {
            for (auto child : adj[node]) {
                if (--in_degree[child] == 0) queue.push_back(child);
            }
        }
    }

    if (processed < in_degree.size()) {
        add_diagnostic(out,
                       ValidationSeverity::Error,
                       "X2",
                       "Signal flow graph contains a cycle among group buses",
                       ErrorCode::SignalFlowCycle);
    }
}

// -------------------------------------------------------------------------
// X3: Every channel reaches the master bus
// -------------------------------------------------------------------------

void check_reachability(const MixGraph& graph, std::vector<Diagnostic>& out) {
    // Build set of group IDs that eventually reach master
    std::unordered_map<std::uint64_t, GroupOutputType> group_output;
    std::unordered_map<std::uint64_t, std::uint64_t> group_parent;

    for (const auto& g : graph.group_buses) {
        group_output[g.id.value] = g.output.type;
        if (g.output.type == GroupOutputType::Group)
            group_parent[g.id.value] = g.output.parent_group_id.value;
    }

    auto reaches_master = [&](std::uint64_t gid) -> bool {
        std::unordered_set<std::uint64_t> visited;
        auto current = gid;
        while (true) {
            if (visited.count(current)) return false; // cycle — handled by X2
            visited.insert(current);
            auto it = group_output.find(current);
            if (it == group_output.end()) return false;
            if (it->second == GroupOutputType::Master) return true;
            auto pit = group_parent.find(current);
            if (pit == group_parent.end()) return false;
            current = pit->second;
        }
    };

    for (const auto& ch : graph.channels) {
        if (ch.group_assignment.has_value()) {
            if (!reaches_master(ch.group_assignment->value)) {
                add_diagnostic(out,
                               ValidationSeverity::Error,
                               "X3",
                               "Channel (part " + std::to_string(ch.part_id.value) +
                                   ") does not reach the master bus",
                               ErrorCode::UnreachableMaster);
            }
        }
        // Ungrouped channels route directly to master — always reachable
    }
}

// -------------------------------------------------------------------------
// X3b: Group bus nesting depth
// -------------------------------------------------------------------------

void check_nesting_depth(const MixGraph& graph, std::vector<Diagnostic>& out) {
    std::unordered_map<std::uint64_t, std::uint64_t> parent_map;
    for (const auto& g : graph.group_buses) {
        if (g.output.type == GroupOutputType::Group)
            parent_map[g.id.value] = g.output.parent_group_id.value;
    }

    for (const auto& g : graph.group_buses) {
        std::uint8_t depth = 0;
        auto current = g.id.value;
        std::unordered_set<std::uint64_t> visited;
        while (parent_map.count(current)) {
            if (visited.count(current)) break; // cycle — handled by X2
            visited.insert(current);
            current = parent_map[current];
            depth++;
        }
        if (depth > graph.max_group_nesting_depth) {
            add_diagnostic(out,
                           ValidationSeverity::Error,
                           "X3b",
                           "Group bus '" + g.name + "' nesting depth (" + std::to_string(depth) +
                               ") exceeds maximum (" +
                               std::to_string(graph.max_group_nesting_depth) + ")",
                           ErrorCode::NestingDepthExceeded);
        }
    }
}

// -------------------------------------------------------------------------
// X4: Channel has no insert processing
// -------------------------------------------------------------------------

void check_no_insert(const MixGraph& graph, std::vector<Diagnostic>& out) {
    for (const auto& ch : graph.channels) {
        if (ch.insert_chain.effects.empty()) {
            add_diagnostic(out,
                           ValidationSeverity::Warning,
                           "X4",
                           "Channel (part " + std::to_string(ch.part_id.value) +
                               ") has no insert processing",
                           ErrorCode::NoInsertProcessing);
        }
    }
}

// -------------------------------------------------------------------------
// X5: Fader at -inf but not muted
// -------------------------------------------------------------------------

void check_silent_not_muted(const MixGraph& graph, std::vector<Diagnostic>& out) {
    constexpr float NEG_INF_THRESHOLD = -96.0f; // Treat below -96 dB as -inf

    std::unordered_map<std::uint64_t, float> effective_levels;
    if (const auto resolution = resolve_relative_levels(graph)) {
        for (const auto& entry : resolution->levels) {
            if (entry.target_type != FaderTargetType::Channel) continue;
            effective_levels.emplace(entry.target_id,
                                     entry.resolved_level_db.value_or(entry.explicit_level_db));
        }
    }

    for (const auto& ch : graph.channels) {
        const auto found = effective_levels.find(ch.id.value);
        const float effective_level =
            found == effective_levels.end() ? ch.fader.level_db : found->second;
        if (effective_level < NEG_INF_THRESHOLD && !ch.mute) {
            add_diagnostic(out,
                           ValidationSeverity::Warning,
                           "X5",
                           "Channel (part " + std::to_string(ch.part_id.value) +
                               ") fader is effectively silent but not muted",
                           ErrorCode::SilentNotMuted);
        }
    }
}

// -------------------------------------------------------------------------
// X9: relative fader constraints are finite, referentially closed, and acyclic
// -------------------------------------------------------------------------

void check_relative_levels(const MixGraph& graph, std::vector<Diagnostic>& out) {
    if (resolve_relative_levels(graph)) return;
    add_diagnostic(out,
                   ValidationSeverity::Error,
                   "X9",
                   "Relative fader constraints contain a missing reference, dependency cycle, "
                   "non-finite value, invalid loudness target, or derived level above +12 dB",
                   ErrorCode::MixInvalidParameter);
}

// -------------------------------------------------------------------------
// X6: Sidechain source references non-existent channel or bus
// -------------------------------------------------------------------------

template <typename Visitor>
void visit_sidechains(const MixEffectParameters& parameters, Visitor&& visitor) {
    std::visit(
        [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, MixCompressor> || std::is_same_v<T, MixGate>) {
                visitor(value.sidechain);
            } else if constexpr (std::is_same_v<T, MixMultibandDynamics>) {
                for (const auto& band : value.bands) {
                    if (band.compressor) visitor(band.compressor->sidechain);
                    if (band.expander) visitor(band.expander->sidechain);
                }
            }
        },
        parameters);
}

void check_sidechain_refs(const MixGraph& graph, std::vector<Diagnostic>& out) {
    std::unordered_set<std::uint64_t> channel_ids;
    std::unordered_set<std::uint64_t> group_ids;

    for (const auto& ch : graph.channels)
        channel_ids.insert(ch.id.value);
    for (const auto& g : graph.group_buses)
        group_ids.insert(g.id.value);

    auto check_chain = [&](const MixEffectChain& chain, const std::string& context) {
        for (const auto& fx : chain.effects) {
            visit_sidechains(fx.parameters, [&](const MixSidechainConfig& sidechain) {
                if (sidechain.source == SidechainSourceType::ExternalChannel &&
                    !channel_ids.contains(sidechain.channel_id.value)) {
                    add_diagnostic(out,
                                   ValidationSeverity::Error,
                                   "X6",
                                   context + " effect " + std::to_string(fx.id.value) +
                                       " sidechain references non-existent channel " +
                                       std::to_string(sidechain.channel_id.value),
                                   ErrorCode::InvalidSidechain);
                }
                if (sidechain.source == SidechainSourceType::ExternalBus &&
                    !group_ids.contains(sidechain.bus_id.value)) {
                    add_diagnostic(out,
                                   ValidationSeverity::Error,
                                   "X6",
                                   context + " effect " + std::to_string(fx.id.value) +
                                       " sidechain references non-existent bus " +
                                       std::to_string(sidechain.bus_id.value),
                                   ErrorCode::InvalidSidechain);
                }
            });
        }
    };

    for (const auto& ch : graph.channels) {
        check_chain(ch.insert_chain, "Channel (part " + std::to_string(ch.part_id.value) + ")");
    }
    for (const auto& g : graph.group_buses) {
        check_chain(g.insert_chain, "GroupBus '" + g.name + "'");
    }
    for (const auto& aux : graph.aux_buses) {
        check_chain(aux.effect_chain, "AuxBus '" + aux.name + "'");
    }
    check_chain(graph.master_bus.insert_chain, "MasterBus");
}

// -------------------------------------------------------------------------
// I1: Channel has no intent
// -------------------------------------------------------------------------

void check_intent_annotations(const MixGraph& graph, std::vector<Diagnostic>& out) {
    for (const auto& ch : graph.channels) {
        if (!ch.intent.has_value()) {
            add_diagnostic(out,
                           ValidationSeverity::Info,
                           "I1",
                           "Channel (part " + std::to_string(ch.part_id.value) +
                               ") has no ChannelIntent annotation",
                           ErrorCode::NoChannelIntent);
        }
    }

    for (const auto& g : graph.group_buses) {
        if (!g.intent.has_value()) {
            add_diagnostic(out,
                           ValidationSeverity::Info,
                           "I2",
                           "GroupBus '" + g.name + "' has no GroupIntent annotation",
                           ErrorCode::NoGroupIntent);
        }
    }
}

// -------------------------------------------------------------------------
// I3: Lead role but fader below average
// -------------------------------------------------------------------------

void check_lead_level(const MixGraph& graph, std::vector<Diagnostic>& out) {
    if (graph.channels.empty()) return;

    std::unordered_map<std::uint64_t, float> effective_levels;
    if (const auto resolution = resolve_relative_levels(graph)) {
        for (const auto& entry : resolution->levels) {
            if (entry.target_type != FaderTargetType::Channel) continue;
            effective_levels.emplace(entry.target_id,
                                     entry.resolved_level_db.value_or(entry.explicit_level_db));
        }
    }

    float sum = 0.0f;
    int count = 0;
    for (const auto& ch : graph.channels) {
        if (!ch.mute) {
            const auto level = effective_levels.find(ch.id.value);
            sum += level == effective_levels.end() ? ch.fader.level_db : level->second;
            count++;
        }
    }
    if (count == 0) return;
    float avg = sum / static_cast<float>(count);

    for (const auto& ch : graph.channels) {
        const auto level = effective_levels.find(ch.id.value);
        const float effective_level =
            level == effective_levels.end() ? ch.fader.level_db : level->second;
        if (ch.intent.has_value() && ch.intent->role_in_mix == MixRole::Lead &&
            effective_level < avg) {
            add_diagnostic(out,
                           ValidationSeverity::Warning,
                           "I3",
                           "Channel (part " + std::to_string(ch.part_id.value) +
                               ") is marked Lead but fader (" +
                               std::to_string(static_cast<int>(effective_level)) +
                               " dB) is below average (" + std::to_string(static_cast<int>(avg)) +
                               " dB)",
                           ErrorCode::LeadTooQuiet);
        }
    }
}

// -------------------------------------------------------------------------
// I4: Foundation role with HPF removing sub-bass
// -------------------------------------------------------------------------

void check_foundation_hpf(const MixGraph& graph, std::vector<Diagnostic>& out) {
    for (const auto& ch : graph.channels) {
        if (!ch.intent.has_value() || ch.intent->role_in_mix != MixRole::Foundation) continue;

        for (const auto& fx : ch.insert_chain.effects) {
            if (!fx.enabled) continue;
            std::visit(
                [&](const auto& p) {
                    using T = std::decay_t<decltype(p)>;
                    if constexpr (std::is_same_v<T, MixEQ>) {
                        for (const auto& band : p.bands) {
                            if (band.band_type == MixEQBandType::HighCut)
                                continue; // HighCut removes highs, not lows
                            if (band.band_type == MixEQBandType::LowCut && band.frequency > 60.0f) {
                                add_diagnostic(
                                    out,
                                    ValidationSeverity::Warning,
                                    "I4",
                                    "Channel (part " + std::to_string(ch.part_id.value) +
                                        ") marked Foundation has HPF at " +
                                        std::to_string(static_cast<int>(band.frequency)) +
                                        " Hz removing sub-bass",
                                    ErrorCode::FoundationNoSubBass);
                            }
                        }
                    }
                },
                fx.parameters);
        }
    }
}

// -------------------------------------------------------------------------
// I5: Flat depth staging (all channels have same depth position)
// -------------------------------------------------------------------------

void check_depth_staging(const MixGraph& graph, std::vector<Diagnostic>& out) {
    if (graph.channels.size() < 2) return;

    bool all_same = true;
    DepthPosition first = DepthPosition::Mid;
    bool has_intent = false;

    for (const auto& ch : graph.channels) {
        if (ch.intent.has_value()) {
            if (!has_intent) {
                first = ch.intent->depth_position;
                has_intent = true;
            } else if (ch.intent->depth_position != first) {
                all_same = false;
                break;
            }
        }
    }

    if (has_intent && all_same) {
        add_diagnostic(out,
                       ValidationSeverity::Info,
                       "I5",
                       "All channel depth positions are identical (flat depth staging)",
                       ErrorCode::FlatDepthStaging);
    }
}

// -------------------------------------------------------------------------
// Aux send target validation
// -------------------------------------------------------------------------

void check_aux_send_targets(const MixGraph& graph, std::vector<Diagnostic>& out) {
    std::unordered_set<std::uint64_t> aux_ids;
    for (const auto& a : graph.aux_buses)
        aux_ids.insert(a.id.value);

    const auto check = [&](const auto& source, const std::string& label) {
        std::unordered_set<std::uint64_t> targets;
        for (const auto& send : source.sends) {
            if (aux_ids.find(send.aux_bus_id.value) == aux_ids.end()) {
                add_diagnostic(out,
                               ValidationSeverity::Error,
                               "X6",
                               label + " sends to non-existent aux bus " +
                                   std::to_string(send.aux_bus_id.value),
                               ErrorCode::InvalidSidechain);
            }
            if (!targets.insert(send.aux_bus_id.value).second) {
                add_diagnostic(out,
                               ValidationSeverity::Error,
                               "X11",
                               label + " contains duplicate sends to aux bus " +
                                   std::to_string(send.aux_bus_id.value),
                               ErrorCode::InvalidAuxSend);
            }
            if (!std::isfinite(send.level_db) || send.level_db > MIX_LEVEL_CEILING_DB) {
                add_diagnostic(out,
                               ValidationSeverity::Error,
                               "X11",
                               label + " has a send level outside (-inf, " +
                                   std::to_string(MIX_LEVEL_CEILING_DB) + "] dB for aux bus " +
                                   std::to_string(send.aux_bus_id.value),
                               ErrorCode::InvalidAuxSend);
            }
        }
    };
    for (const auto& channel : graph.channels)
        check(channel, "Channel " + std::to_string(channel.id.value));
    for (const auto& group : graph.group_buses)
        check(group, "Group " + std::to_string(group.id.value));
}

// -------------------------------------------------------------------------
// X12: automation lanes reference valid targets and ScoreTime positions
// -------------------------------------------------------------------------

struct AutomationFailure {
    ErrorCode code;
    std::string message;
};

std::optional<AutomationFailure> automation_failure(const MixGraph& graph,
                                                    const MixAutomation& automation) {
    if (!get_mix_automation_target(graph, automation.target))
        return AutomationFailure{ErrorCode::InvalidPath,
                                 "Automation target '" + automation.target +
                                     "' does not resolve to a parameter in this MixGraph"};
    if (static_cast<std::uint8_t>(automation.interpolation) >
        static_cast<std::uint8_t>(InterpolationMode::Exponential))
        return AutomationFailure{ErrorCode::MixInvalidParameter,
                                 "Automation interpolation is not a defined value"};
    if (automation.breakpoints.empty())
        return AutomationFailure{ErrorCode::MixInvalidParameter,
                                 "Automation must contain at least one breakpoint"};
    for (std::size_t i = 0; i < automation.breakpoints.size(); ++i) {
        const auto& breakpoint = automation.breakpoints[i];
        const auto label = "Automation breakpoint[" + std::to_string(i) + "]";
        if (breakpoint.time < SCORE_START)
            return AutomationFailure{ErrorCode::InvalidScoreTime,
                                     label + " precedes the first valid ScoreTime"};
        if (!std::isfinite(breakpoint.value))
            return AutomationFailure{ErrorCode::MixInvalidParameter,
                                     label + " has a non-finite value"};
        if (i != 0 && breakpoint.time <= automation.breakpoints[i - 1].time)
            return AutomationFailure{ErrorCode::InvalidScoreTime,
                                     "Automation breakpoint times must be strictly increasing"};
    }
    return std::nullopt;
}

void check_automation(const MixGraph& graph, std::vector<Diagnostic>& out) {
    for (const auto& automation : graph.automation) {
        if (auto failure = automation_failure(graph, automation))
            add_diagnostic(out, ValidationSeverity::Error, "X12", failure->message, failure->code);
    }
}

// -------------------------------------------------------------------------
// X13: the master loudness target lies inside the BS.1770 domain
// -------------------------------------------------------------------------

void check_loudness_target(const MixGraph& graph, std::vector<Diagnostic>& out) {
    const auto& target = graph.master_bus.target_loudness;
    if (target && !is_loudness_target_valid(*target))
        add_diagnostic(out,
                       ValidationSeverity::Error,
                       "X13",
                       "Master loudness target requires integrated loudness <= 0 LUFS, true "
                       "peak <= 0 dBTP and a finite non-negative loudness range",
                       ErrorCode::MixInvalidParameter);
}

// -------------------------------------------------------------------------
// X7: Mix effect parameters remain inside their representable domains
// -------------------------------------------------------------------------

bool finite_range(float value, float minimum, float maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}

template <typename E> bool defined(E value, E last) {
    using Underlying = std::underlying_type_t<E>;
    const auto raw = static_cast<Underlying>(value);
    if constexpr (std::is_signed_v<Underlying>)
        if (raw < 0) return false;
    return raw <= static_cast<Underlying>(last);
}

std::optional<std::string> sidechain_parameter_reason(const MixSidechainConfig& sidechain) {
    if (!defined(sidechain.source, SidechainSourceType::ExternalBus))
        return "sidechain source is not a defined value";
    if (!sidechain.filter) return std::nullopt;
    if (!defined(sidechain.filter->filter_type, MixEQBandType::TiltShelf))
        return "sidechain filter type is not a defined value";
    if (!finite_range(sidechain.filter->frequency, 20.0f, 20000.0f))
        return "sidechain filter frequency must be in [20, 20000] Hz";
    if (!finite_range(sidechain.filter->q, std::numeric_limits<float>::min(), 100.0f))
        return "sidechain filter Q must be in (0, 100]";
    return std::nullopt;
}

std::optional<std::string> compressor_parameter_reason(const MixCompressor& value) {
    if (!defined(value.detection, DetectionMode::Envelope) ||
        !defined(value.topology, CompressorTopology::FeedBack))
        return "compressor detection or topology is not a defined value";
    if (!finite_range(value.threshold, -160.0f, 60.0f))
        return "compressor threshold must be in [-160, 60] dB";
    if (!finite_range(value.ratio, 1.0f, 1000.0f)) return "compressor ratio must be in [1, 1000]";
    if (!finite_range(value.attack, 0.0f, 60000.0f))
        return "compressor attack must be in [0, 60000] ms";
    if (!finite_range(value.release, std::numeric_limits<float>::min(), 60000.0f))
        return "compressor release must be in (0, 60000] ms";
    if (!finite_range(value.knee, 0.0f, 60.0f)) return "compressor knee must be in [0, 60] dB";
    if (!finite_range(value.makeup_gain, -120.0f, 120.0f))
        return "compressor makeup gain must be in [-120, 120] dB";
    if (!finite_range(value.stereo_link, 0.0f, 1.0f))
        return "compressor stereo link must be in [0, 1]";
    return sidechain_parameter_reason(value.sidechain);
}

std::optional<std::string> gate_parameter_reason(const MixGate& value) {
    if (!finite_range(value.threshold, -160.0f, 60.0f))
        return "gate threshold must be in [-160, 60] dB";
    if (!finite_range(value.ratio, 1.0f, 1000.0f)) return "gate ratio must be in [1, 1000]";
    if (!finite_range(value.attack, 0.0f, 60000.0f)) return "gate attack must be in [0, 60000] ms";
    if (!finite_range(value.hold, 0.0f, 60000.0f)) return "gate hold must be in [0, 60000] ms";
    if (!finite_range(value.release, std::numeric_limits<float>::min(), 60000.0f))
        return "gate release must be in (0, 60000] ms";
    if (!finite_range(value.range, -160.0f, 0.0f))
        return "gate attenuation range must be in [-160, 0] dB";
    return sidechain_parameter_reason(value.sidechain);
}

void check_effect_parameters(const MixGraph& graph, std::vector<Diagnostic>& out) {
    for (const auto& channel : graph.channels)
        if (!finite_range(channel.input_trim, -24.0f, 24.0f))
            add_diagnostic(out,
                           ValidationSeverity::Error,
                           "X7",
                           "Channel input trim must be finite and in [-24, +24] dB",
                           ErrorCode::MixInvalidParameter);
    auto unit_interval = [](float value) {
        return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
    };

    auto check_chain = [&](const MixEffectChain& chain, const std::string& context) {
        for (const auto& effect : chain.effects) {
            std::optional<std::string> reason;
            std::visit(
                [&](const auto& parameters) {
                    using T = std::decay_t<decltype(parameters)>;
                    if constexpr (std::is_same_v<T, MixEQ>) {
                        if (parameters.bands.size() > 8) {
                            reason = "EQ supports at most eight bands";
                        } else {
                            for (const auto& band : parameters.bands) {
                                if (!defined(band.band_type, MixEQBandType::TiltShelf)) {
                                    reason = "EQ band type is not a defined value";
                                    break;
                                }
                                if (!finite_range(band.frequency, 20.0f, 20000.0f)) {
                                    reason = "EQ frequency must be in [20, 20000] Hz";
                                    break;
                                }
                                if (!finite_range(band.gain, -120.0f, 120.0f)) {
                                    reason = "EQ gain must be in [-120, 120] dB";
                                    break;
                                }
                                if (!finite_range(
                                        band.q, std::numeric_limits<float>::min(), 100.0f)) {
                                    reason = "EQ Q must be in (0, 100]";
                                    break;
                                }
                                if (band.dynamic &&
                                    (!finite_range(band.dynamic->threshold, -160.0f, 60.0f) ||
                                     !finite_range(band.dynamic->ratio, 1.0f, 1000.0f) ||
                                     !finite_range(band.dynamic->attack, 0.0f, 60000.0f) ||
                                     !finite_range(band.dynamic->release,
                                                   std::numeric_limits<float>::min(),
                                                   60000.0f))) {
                                    reason = "dynamic EQ threshold, ratio, attack, or release is "
                                             "outside its representable domain";
                                    break;
                                }
                            }
                        }
                    } else if constexpr (std::is_same_v<T, MixCompressor>) {
                        reason = compressor_parameter_reason(parameters);
                    } else if constexpr (std::is_same_v<T, MixGate>) {
                        reason = gate_parameter_reason(parameters);
                    } else if constexpr (std::is_same_v<T, MixLimiter>) {
                        if (!defined(parameters.algorithm, LimiterAlgorithm::ISP))
                            reason = "limiter algorithm is not a defined value";
                        else if (!finite_range(parameters.ceiling, -60.0f, 6.0f))
                            reason = "limiter ceiling must be in [-60, 6] dBFS";
                        else if (!finite_range(parameters.release,
                                               std::numeric_limits<float>::min(),
                                               60000.0f))
                            reason = "limiter release must be in (0, 60000] ms";
                        else if (!finite_range(parameters.lookahead, 0.0f, 1000.0f))
                            reason = "limiter lookahead must be in [0, 1000] ms";
                    } else if constexpr (std::is_same_v<T, MixMultibandDynamics>) {
                        if (!defined(parameters.crossover_slope, CrossoverSlope::LinearPhase))
                            reason = "crossover slope is not a defined value";
                        else if (parameters.crossover_frequencies.size() > 7)
                            reason = "multiband dynamics supports at most seven crossovers";
                        else if ((!parameters.crossover_frequencies.empty() ||
                                  !parameters.bands.empty()) &&
                                 parameters.bands.size() !=
                                     parameters.crossover_frequencies.size() + 1)
                            reason = "multiband band count must equal crossover count plus one";
                        float previous = 0.0f;
                        for (const auto frequency : parameters.crossover_frequencies) {
                            if (reason) break;
                            if (!finite_range(frequency, 20.0f, 20000.0f) || frequency <= previous)
                                reason = "multiband crossovers must be strictly increasing in "
                                         "[20, 20000] Hz";
                            previous = frequency;
                        }
                        for (const auto& band : parameters.bands) {
                            if (reason) break;
                            if (!finite_range(band.gain, -120.0f, 120.0f))
                                reason = "multiband band gain must be in [-120, 120] dB";
                            else if (band.compressor)
                                reason = compressor_parameter_reason(*band.compressor);
                            if (!reason && band.expander)
                                reason = gate_parameter_reason(*band.expander);
                        }
                    } else if constexpr (std::is_same_v<T, MixSaturation>) {
                        if (!defined(parameters.algorithm.type, SaturationTypeTag::Hard) ||
                            !defined(parameters.algorithm.tape_speed, TapeSpeed::Ips30) ||
                            !defined(parameters.algorithm.console_type, ConsoleType::Generic))
                            reason = "saturation algorithm is not a defined value";
                        else if (!unit_interval(parameters.drive) || !unit_interval(parameters.mix))
                            reason = "saturation drive and mix must be in [0, 1]";
                        else if (!finite_range(parameters.output_level, -120.0f, 120.0f))
                            reason = "saturation output level must be in [-120, 120] dB";
                        else if (parameters.algorithm.type == SaturationTypeTag::Tape &&
                                 !finite_range(parameters.algorithm.tape_bias, -1.0f, 1.0f))
                            reason = "saturation tape bias must be in [-1, 1]";
                        else if (parameters.algorithm.type == SaturationTypeTag::Tube &&
                                 parameters.algorithm.tube_model.empty())
                            reason = "tube saturation requires a non-empty tube model";
                    } else if constexpr (std::is_same_v<T, MixStereoProcessor>) {
                        if (!finite_range(parameters.width, 0.0f, 4.0f))
                            reason = "stereo width must be in [0, 4]";
                        else if (!unit_interval(parameters.mid_side_balance))
                            reason = "mid/side balance must be in [0, 1]";
                        else if (parameters.mono_below &&
                                 !finite_range(*parameters.mono_below, 20.0f, 20000.0f))
                            reason = "mono-below frequency must be in [20, 20000] Hz";
                    } else if constexpr (std::is_same_v<T, MixDelay>) {
                        if (!defined(parameters.stereo_mode, MixDelayMode::PingPong))
                            reason = "delay stereo mode is not a defined value";
                        else if (!parameters.tempo_synced &&
                                 (!std::isfinite(parameters.delay_ms) ||
                                  parameters.delay_ms <= 0.0f || parameters.delay_ms > 10000.0f))
                            reason = "delay time must be in (0, 10000] ms";
                        else if (parameters.tempo_synced &&
                                 parameters.beat_division.numerator() <= 0)
                            reason = "delay beat division must be positive";
                        else if (!unit_interval(parameters.feedback))
                            reason = "delay feedback must be in [0, 1]";
                        else if (!std::isfinite(parameters.stereo_offset) ||
                                 parameters.stereo_offset < 0.0f ||
                                 parameters.stereo_offset > 1000.0f)
                            reason = "delay stereo offset must be in [0, 1000]";
                        else if (!std::isfinite(parameters.low_cut_hz) ||
                                 !std::isfinite(parameters.high_cut_hz) ||
                                 parameters.low_cut_hz < 20.0f ||
                                 parameters.high_cut_hz > 20000.0f ||
                                 parameters.low_cut_hz >= parameters.high_cut_hz)
                            reason =
                                "delay filter bounds must satisfy 20 <= low < high <= 20000 Hz";
                        else if (!std::isfinite(parameters.modulation_rate) ||
                                 parameters.modulation_rate < 0.0f ||
                                 parameters.modulation_rate > 20.0f)
                            reason = "delay modulation rate must be in [0, 20] Hz";
                        else if (!std::isfinite(parameters.modulation_depth) ||
                                 parameters.modulation_depth < 0.0f ||
                                 parameters.modulation_depth > 100.0f)
                            reason = "delay modulation depth must be in [0, 100] ms";
                        else if (!unit_interval(parameters.mix))
                            reason = "delay mix must be in [0, 1]";
                    } else if constexpr (std::is_same_v<T, MixReverb>) {
                        if (!defined(parameters.algorithm, MixReverbAlgorithm::Shimmer))
                            reason = "reverb algorithm is not a defined value";
                        else if (parameters.algorithm == MixReverbAlgorithm::Convolution &&
                                 parameters.impulse_response.empty())
                            reason = "convolution reverb requires an impulse response";
                        else if (parameters.algorithm == MixReverbAlgorithm::Shimmer &&
                                 (!std::isfinite(parameters.shimmer_pitch) ||
                                  parameters.shimmer_pitch < -24.0f ||
                                  parameters.shimmer_pitch > 24.0f))
                            reason = "reverb shimmer pitch must be in [-24, 24] semitones";
                        else if (!std::isfinite(parameters.decay_time) ||
                                 parameters.decay_time <= 0.0f || parameters.decay_time > 120.0f)
                            reason = "reverb decay must be in (0, 120] seconds";
                        else if (!std::isfinite(parameters.pre_delay) ||
                                 parameters.pre_delay < 0.0f || parameters.pre_delay > 500.0f)
                            reason = "reverb pre-delay must be in [0, 500] ms";
                        else if (!unit_interval(parameters.damping) ||
                                 !unit_interval(parameters.diffusion) ||
                                 !unit_interval(parameters.size) ||
                                 !unit_interval(parameters.early_reflections_level) ||
                                 !unit_interval(parameters.mix))
                            reason = "reverb normalised parameters must be in [0, 1]";
                        else if (!std::isfinite(parameters.low_cut_hz) ||
                                 !std::isfinite(parameters.high_cut_hz) ||
                                 parameters.low_cut_hz < 20.0f ||
                                 parameters.high_cut_hz > 20000.0f ||
                                 parameters.low_cut_hz >= parameters.high_cut_hz)
                            reason =
                                "reverb filter bounds must satisfy 20 <= low < high <= 20000 Hz";
                    }
                },
                effect.parameters);

            if (reason) {
                add_diagnostic(out,
                               ValidationSeverity::Error,
                               "X7",
                               context + " effect " + std::to_string(effect.id.value) + ": " +
                                   *reason,
                               ErrorCode::MixInvalidParameter);
            }
        }
    };

    for (const auto& channel : graph.channels)
        check_chain(channel.insert_chain, "Channel " + std::to_string(channel.id.value));
    for (const auto& group : graph.group_buses)
        check_chain(group.insert_chain, "Group bus " + std::to_string(group.id.value));
    for (const auto& aux : graph.aux_buses)
        check_chain(aux.effect_chain, "Aux bus " + std::to_string(aux.id.value));
    check_chain(graph.master_bus.insert_chain, "Master bus");
}

// -------------------------------------------------------------------------
// X8: target parameter mappings resolve and are non-aliasing
// -------------------------------------------------------------------------

void check_parameter_mappings(const MixGraph& graph, std::vector<Diagnostic>& out) {
    const auto check_chain = [&](const MixEffectChain& chain, const std::string& context) {
        for (const auto& effect : chain.effects) {
            std::unordered_set<std::string> target_names;
            for (const auto& [path, mapping] : effect.parameter_map) {
                const auto source = get_mix_effect_parameter(effect, path);
                const auto target = source ? map_mix_device_parameter_value(*source, mapping)
                                           : Result<float>{std::unexpected(source.error())};
                if (path.empty() || mapping.parameter_name.empty() || !source || !target ||
                    !target_names.insert(mapping.parameter_name).second) {
                    add_diagnostic(out,
                                   ValidationSeverity::Error,
                                   "X8",
                                   context + " effect " + std::to_string(effect.id.value) +
                                       " has an invalid or aliasing target mapping for '" + path +
                                       "'",
                                   ErrorCode::MixInvalidParameter);
                }
            }
        }
    };

    for (const auto& channel : graph.channels)
        check_chain(channel.insert_chain, "Channel " + std::to_string(channel.id.value));
    for (const auto& group : graph.group_buses)
        check_chain(group.insert_chain, "Group bus " + std::to_string(group.id.value));
    for (const auto& aux : graph.aux_buses)
        check_chain(aux.effect_chain, "Aux bus " + std::to_string(aux.id.value));
    check_chain(graph.master_bus.insert_chain, "Master bus");
}

} // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

std::vector<Diagnostic> validate_mix(const MixGraph& graph) {
    std::vector<Diagnostic> diags;

    // Structural rules
    check_unique_ids(graph, diags);
    check_routing_membership_consistency(graph, diags);
    check_dag_acyclicity(graph, diags);
    check_reachability(graph, diags);
    check_nesting_depth(graph, diags);
    check_no_insert(graph, diags);
    check_silent_not_muted(graph, diags);
    check_sidechain_refs(graph, diags);
    check_aux_send_targets(graph, diags);
    check_effect_parameters(graph, diags);
    check_parameter_mappings(graph, diags);
    check_relative_levels(graph, diags);
    check_automation(graph, diags);
    check_loudness_target(graph, diags);

    // Intent rules
    check_intent_annotations(graph, diags);
    check_lead_level(graph, diags);
    check_foundation_hpf(graph, diags);
    check_depth_staging(graph, diags);

    // Sort: Error first, then Warning, then Info
    std::sort(diags.begin(), diags.end(), [](const Diagnostic& a, const Diagnostic& b) {
        return static_cast<int>(a.severity) < static_cast<int>(b.severity);
    });

    return diags;
}

std::vector<Diagnostic> validate_mix_correspondence(const Score& score, const MixGraph& graph) {
    std::vector<Diagnostic> diags;

    std::unordered_set<std::uint64_t> score_parts;
    for (const auto& part : score.parts)
        score_parts.insert(part.id.value);

    std::unordered_map<std::uint64_t, std::size_t> channel_counts;
    for (const auto& ch : graph.channels) {
        ++channel_counts[ch.part_id.value];
        if (!score_parts.contains(ch.part_id.value)) {
            add_diagnostic(diags,
                           ValidationSeverity::Error,
                           "X1",
                           "Channel " + std::to_string(ch.id.value) + " references unknown Part " +
                               std::to_string(ch.part_id.value),
                           ErrorCode::UnknownChannelPart,
                           ch.part_id);
        }
    }

    for (const auto& part : score.parts) {
        const auto count = channel_counts[part.id.value];
        if (count == 0) {
            add_diagnostic(diags,
                           ValidationSeverity::Error,
                           "X1",
                           "Part '" + part.definition.name + "' has no corresponding ChannelStrip",
                           ErrorCode::MissingChannel,
                           part.id);
        } else if (count > 1) {
            add_diagnostic(diags,
                           ValidationSeverity::Error,
                           "X1",
                           "Part '" + part.definition.name + "' has " + std::to_string(count) +
                               " ChannelStrips; exactly one is required",
                           ErrorCode::DuplicateChannelForPart,
                           part.id);
        }
    }

    return diags;
}

Result<void> validate_mix_automation(const MixGraph& graph, const MixAutomation& automation) {
    if (auto failure = automation_failure(graph, automation)) return std::unexpected(failure->code);
    return {};
}

bool is_loudness_target_valid(const LoudnessTarget& target) {
    if (!std::isfinite(target.integrated_lufs) || target.integrated_lufs > 0.0f) return false;
    if (!std::isfinite(target.true_peak_dbfs) || target.true_peak_dbfs > 0.0f) return false;
    if (target.loudness_range_lu &&
        (!std::isfinite(*target.loudness_range_lu) || *target.loudness_range_lu < 0.0f))
        return false;
    return true;
}

bool is_mix_valid(const MixGraph& graph) {
    auto diags = validate_mix(graph);
    return std::none_of(diags.begin(), diags.end(), [](const Diagnostic& d) {
        return d.severity == ValidationSeverity::Error;
    });
}

} // namespace sunny::core
