/**
 * @file MIVD001A.cpp
 * @brief Mix IR validation implementation
 *
 * Component: MIVD001A
 * Domain: MI (Mix IR) | Category: VD (Validation)
 *
 * Tests: TSMI003A
 */

#include "MIVD001A.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace Sunny::Core {

namespace {

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

void add_diagnostic(std::vector<Diagnostic>& out,
                    ValidationSeverity sev,
                    const char* rule,
                    const std::string& msg,
                    int code) {
    out.push_back({sev, rule, msg, std::nullopt, std::nullopt, code});
}

// -------------------------------------------------------------------------
// X2: Signal flow DAG — acyclicity check
//
// Builds the group-to-group adjacency from member_groups and output
// routing, then runs a topological sort (Kahn's algorithm). If any
// nodes remain after the sort, a cycle exists.
// -------------------------------------------------------------------------

void check_dag_acyclicity(const MixGraph& graph,
                          std::vector<Diagnostic>& out) {
    // Build adjacency: parent → child for member_groups; child → parent for output.Group
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> adj;
    std::unordered_map<std::uint64_t, int> in_degree;

    for (const auto& g : graph.group_buses) {
        if (in_degree.find(g.id.value) == in_degree.end())
            in_degree[g.id.value] = 0;
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
                if (--in_degree[child] == 0)
                    queue.push_back(child);
            }
        }
    }

    if (processed < in_degree.size()) {
        add_diagnostic(out, ValidationSeverity::Error, "X2",
            "Signal flow graph contains a cycle among group buses",
            MixError::SignalFlowCycle);
    }
}

// -------------------------------------------------------------------------
// X3: Every channel reaches the master bus
// -------------------------------------------------------------------------

void check_reachability(const MixGraph& graph,
                        std::vector<Diagnostic>& out) {
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
            if (visited.count(current)) return false;  // cycle — handled by X2
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
                add_diagnostic(out, ValidationSeverity::Error, "X3",
                    "Channel (part " + std::to_string(ch.part_id.value) +
                    ") does not reach the master bus",
                    MixError::UnreachableMaster);
            }
        }
        // Ungrouped channels route directly to master — always reachable
    }
}

// -------------------------------------------------------------------------
// X3b: Group bus nesting depth
// -------------------------------------------------------------------------

void check_nesting_depth(const MixGraph& graph,
                         std::vector<Diagnostic>& out) {
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
            if (visited.count(current)) break;  // cycle — handled by X2
            visited.insert(current);
            current = parent_map[current];
            depth++;
        }
        if (depth > graph.max_group_nesting_depth) {
            add_diagnostic(out, ValidationSeverity::Error, "X3b",
                "Group bus '" + g.name + "' nesting depth (" +
                std::to_string(depth) + ") exceeds maximum (" +
                std::to_string(graph.max_group_nesting_depth) + ")",
                MixError::NestingDepthExceeded);
        }
    }
}

// -------------------------------------------------------------------------
// X4: Channel has no insert processing
// -------------------------------------------------------------------------

void check_no_insert(const MixGraph& graph,
                     std::vector<Diagnostic>& out) {
    for (const auto& ch : graph.channels) {
        if (ch.insert_chain.effects.empty()) {
            add_diagnostic(out, ValidationSeverity::Warning, "X4",
                "Channel (part " + std::to_string(ch.part_id.value) +
                ") has no insert processing",
                MixError::NoInsertProcessing);
        }
    }
}

// -------------------------------------------------------------------------
// X5: Fader at -inf but not muted
// -------------------------------------------------------------------------

void check_silent_not_muted(const MixGraph& graph,
                            std::vector<Diagnostic>& out) {
    constexpr float NEG_INF_THRESHOLD = -96.0f;  // Treat below -96 dB as -inf

    for (const auto& ch : graph.channels) {
        if (ch.fader.level_db < NEG_INF_THRESHOLD && !ch.mute) {
            add_diagnostic(out, ValidationSeverity::Warning, "X5",
                "Channel (part " + std::to_string(ch.part_id.value) +
                ") fader is effectively silent but not muted",
                MixError::SilentNotMuted);
        }
    }
}

// -------------------------------------------------------------------------
// X6: Sidechain source references non-existent channel or bus
// -------------------------------------------------------------------------

void check_sidechain_refs(const MixGraph& graph,
                          std::vector<Diagnostic>& out) {
    std::unordered_set<std::uint64_t> channel_ids;
    std::unordered_set<std::uint64_t> group_ids;

    for (const auto& ch : graph.channels) channel_ids.insert(ch.id.value);
    for (const auto& g : graph.group_buses) group_ids.insert(g.id.value);

    auto check_chain = [&](const MixEffectChain& chain, const std::string& context) {
        for (const auto& fx : chain.effects) {
            std::visit([&](const auto& p) {
                using T = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<T, MixCompressor>) {
                    if (p.sidechain.source == SidechainSourceType::ExternalChannel) {
                        if (channel_ids.find(p.sidechain.channel_id.value) == channel_ids.end()) {
                            add_diagnostic(out, ValidationSeverity::Error, "X6",
                                context + " sidechain references non-existent channel " +
                                std::to_string(p.sidechain.channel_id.value),
                                MixError::InvalidSidechain);
                        }
                    }
                    if (p.sidechain.source == SidechainSourceType::ExternalBus) {
                        if (group_ids.find(p.sidechain.bus_id.value) == group_ids.end()) {
                            add_diagnostic(out, ValidationSeverity::Error, "X6",
                                context + " sidechain references non-existent bus " +
                                std::to_string(p.sidechain.bus_id.value),
                                MixError::InvalidSidechain);
                        }
                    }
                }
                else if constexpr (std::is_same_v<T, MixGate>) {
                    if (p.sidechain.source == SidechainSourceType::ExternalChannel) {
                        if (channel_ids.find(p.sidechain.channel_id.value) == channel_ids.end()) {
                            add_diagnostic(out, ValidationSeverity::Error, "X6",
                                context + " gate sidechain references non-existent channel " +
                                std::to_string(p.sidechain.channel_id.value),
                                MixError::InvalidSidechain);
                        }
                    }
                    if (p.sidechain.source == SidechainSourceType::ExternalBus) {
                        if (group_ids.find(p.sidechain.bus_id.value) == group_ids.end()) {
                            add_diagnostic(out, ValidationSeverity::Error, "X6",
                                context + " gate sidechain references non-existent bus " +
                                std::to_string(p.sidechain.bus_id.value),
                                MixError::InvalidSidechain);
                        }
                    }
                }
            }, fx.parameters);
        }
    };

    for (const auto& ch : graph.channels) {
        check_chain(ch.insert_chain,
            "Channel (part " + std::to_string(ch.part_id.value) + ")");
    }
    for (const auto& g : graph.group_buses) {
        check_chain(g.insert_chain, "GroupBus '" + g.name + "'");
    }
    check_chain(graph.master_bus.insert_chain, "MasterBus");
}

// -------------------------------------------------------------------------
// I1: Channel has no intent
// -------------------------------------------------------------------------

void check_intent_annotations(const MixGraph& graph,
                               std::vector<Diagnostic>& out) {
    for (const auto& ch : graph.channels) {
        if (!ch.intent.has_value()) {
            add_diagnostic(out, ValidationSeverity::Info, "I1",
                "Channel (part " + std::to_string(ch.part_id.value) +
                ") has no ChannelIntent annotation",
                MixError::NoChannelIntent);
        }
    }

    for (const auto& g : graph.group_buses) {
        if (!g.intent.has_value()) {
            add_diagnostic(out, ValidationSeverity::Info, "I2",
                "GroupBus '" + g.name + "' has no GroupIntent annotation",
                MixError::NoGroupIntent);
        }
    }
}

// -------------------------------------------------------------------------
// I3: Lead role but fader below average
// -------------------------------------------------------------------------

void check_lead_level(const MixGraph& graph,
                      std::vector<Diagnostic>& out) {
    if (graph.channels.empty()) return;

    float sum = 0.0f;
    int count = 0;
    for (const auto& ch : graph.channels) {
        if (!ch.mute) {
            sum += ch.fader.level_db;
            count++;
        }
    }
    if (count == 0) return;
    float avg = sum / static_cast<float>(count);

    for (const auto& ch : graph.channels) {
        if (ch.intent.has_value() &&
            ch.intent->role_in_mix == MixRole::Lead &&
            ch.fader.level_db < avg) {
            add_diagnostic(out, ValidationSeverity::Warning, "I3",
                "Channel (part " + std::to_string(ch.part_id.value) +
                ") is marked Lead but fader (" +
                std::to_string(static_cast<int>(ch.fader.level_db)) +
                " dB) is below average (" +
                std::to_string(static_cast<int>(avg)) + " dB)",
                MixError::LeadTooQuiet);
        }
    }
}

// -------------------------------------------------------------------------
// I4: Foundation role with HPF removing sub-bass
// -------------------------------------------------------------------------

void check_foundation_hpf(const MixGraph& graph,
                           std::vector<Diagnostic>& out) {
    for (const auto& ch : graph.channels) {
        if (!ch.intent.has_value() ||
            ch.intent->role_in_mix != MixRole::Foundation)
            continue;

        for (const auto& fx : ch.insert_chain.effects) {
            if (!fx.enabled) continue;
            std::visit([&](const auto& p) {
                using T = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<T, MixEQ>) {
                    for (const auto& band : p.bands) {
                        if (band.band_type == MixEQBandType::HighCut)
                            continue;  // HighCut removes highs, not lows
                        if (band.band_type == MixEQBandType::LowCut &&
                            band.frequency > 60.0f) {
                            add_diagnostic(out, ValidationSeverity::Warning, "I4",
                                "Channel (part " + std::to_string(ch.part_id.value) +
                                ") marked Foundation has HPF at " +
                                std::to_string(static_cast<int>(band.frequency)) +
                                " Hz removing sub-bass",
                                MixError::FoundationNoSubBass);
                        }
                    }
                }
            }, fx.parameters);
        }
    }
}

// -------------------------------------------------------------------------
// I5: Flat depth staging (all channels have same depth position)
// -------------------------------------------------------------------------

void check_depth_staging(const MixGraph& graph,
                         std::vector<Diagnostic>& out) {
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
        add_diagnostic(out, ValidationSeverity::Info, "I5",
            "All channel depth positions are identical (flat depth staging)",
            MixError::FlatDepthStaging);
    }
}

// -------------------------------------------------------------------------
// Aux send target validation
// -------------------------------------------------------------------------

void check_aux_send_targets(const MixGraph& graph,
                            std::vector<Diagnostic>& out) {
    std::unordered_set<std::uint64_t> aux_ids;
    for (const auto& a : graph.aux_buses) aux_ids.insert(a.id.value);

    for (const auto& ch : graph.channels) {
        for (const auto& send : ch.sends) {
            if (aux_ids.find(send.aux_bus_id.value) == aux_ids.end()) {
                add_diagnostic(out, ValidationSeverity::Error, "X6",
                    "Channel (part " + std::to_string(ch.part_id.value) +
                    ") sends to non-existent aux bus " +
                    std::to_string(send.aux_bus_id.value),
                    MixError::InvalidSidechain);
            }
        }
    }
}

}  // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

std::vector<Diagnostic> validate_mix(const MixGraph& graph) {
    std::vector<Diagnostic> diags;

    // Structural rules
    check_dag_acyclicity(graph, diags);
    check_reachability(graph, diags);
    check_nesting_depth(graph, diags);
    check_no_insert(graph, diags);
    check_silent_not_muted(graph, diags);
    check_sidechain_refs(graph, diags);
    check_aux_send_targets(graph, diags);

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

std::vector<Diagnostic> validate_mix_correspondence(
    const Score& score,
    const MixGraph& graph
) {
    std::vector<Diagnostic> diags;

    std::unordered_set<std::uint64_t> covered;
    for (const auto& ch : graph.channels) {
        covered.insert(ch.part_id.value);
    }

    for (const auto& part : score.parts) {
        if (covered.find(part.id.value) == covered.end()) {
            add_diagnostic(diags, ValidationSeverity::Error, "X1",
                "Part '" + part.definition.name +
                "' has no corresponding ChannelStrip",
                MixError::MissingChannel);
        }
    }

    return diags;
}

bool is_mix_valid(const MixGraph& graph) {
    auto diags = validate_mix(graph);
    return std::none_of(diags.begin(), diags.end(), [](const Diagnostic& d) {
        return d.severity == ValidationSeverity::Error;
    });
}

}  // namespace Sunny::Core
