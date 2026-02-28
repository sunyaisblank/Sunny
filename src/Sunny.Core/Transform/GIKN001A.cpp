/**
 * @file GIKN001A.cpp
 * @brief Klumpenhouwer Networks implementation
 *
 * Component: GIKN001A
 */

#include "GIKN001A.h"

#include <algorithm>
#include <functional>
#include <vector>

namespace Sunny::Core {

bool knet_edges_consistent(const KNet& net) {
    for (const auto& edge : net.edges) {
        if (edge.from >= net.nodes.size() || edge.to >= net.nodes.size()) {
            return false;
        }
        PitchClass expected = edge.label.apply(net.nodes[edge.from]);
        if (expected != net.nodes[edge.to]) {
            return false;
        }
    }
    return true;
}

namespace {

// Build adjacency list from edges
std::vector<std::vector<std::size_t>> build_adjacency(
    std::size_t node_count,
    const std::vector<KNetEdge>& edges
) {
    std::vector<std::vector<std::size_t>> adj(node_count);
    for (std::size_t i = 0; i < edges.size(); ++i) {
        adj[edges[i].from].push_back(i);
    }
    return adj;
}

// Check all cycles via DFS; returns false if any cycle's label composition != identity
bool check_cycles_from(
    std::size_t start,
    const KNet& net,
    const std::vector<std::vector<std::size_t>>& adj
) {
    // DFS to find all paths from start back to start
    // For typical K-nets (3–6 nodes), this is tractable.
    struct Frame {
        std::size_t node;
        TILabel accumulated;
        std::vector<bool> visited_edges;
    };

    std::vector<Frame> stack;
    Frame init;
    init.node = start;
    init.accumulated = TILabel::identity();
    init.visited_edges.assign(net.edges.size(), false);
    stack.push_back(std::move(init));

    while (!stack.empty()) {
        auto current = std::move(stack.back());
        stack.pop_back();

        for (std::size_t ei : adj[current.node]) {
            if (current.visited_edges[ei]) continue;

            const auto& edge = net.edges[ei];
            TILabel new_acc = edge.label.compose(current.accumulated);

            if (edge.to == start) {
                // Found a cycle — check if composition is identity
                if (!new_acc.is_identity()) {
                    return false;
                }
            } else {
                Frame next;
                next.node = edge.to;
                next.accumulated = new_acc;
                next.visited_edges = current.visited_edges;
                next.visited_edges[ei] = true;
                stack.push_back(std::move(next));
            }
        }
    }
    return true;
}

}  // namespace

bool knet_well_formed(const KNet& net) {
    // First check edge consistency
    if (!knet_edges_consistent(net)) return false;

    // Check cycle composition for all possible starting nodes
    auto adj = build_adjacency(net.nodes.size(), net.edges);
    for (std::size_t i = 0; i < net.nodes.size(); ++i) {
        if (!check_cycles_from(i, net, adj)) {
            return false;
        }
    }
    return true;
}

bool knet_isographic(const KNet& a, const KNet& b) {
    if (a.nodes.size() != b.nodes.size()) return false;
    if (a.edges.size() != b.edges.size()) return false;

    // Check edge topology and kind match
    for (std::size_t i = 0; i < a.edges.size(); ++i) {
        if (a.edges[i].from != b.edges[i].from) return false;
        if (a.edges[i].to != b.edges[i].to) return false;
        if (a.edges[i].label.kind != b.edges[i].label.kind) return false;
    }
    return true;
}

bool knet_strongly_isographic(const KNet& a, const KNet& b) {
    if (!knet_isographic(a, b)) return false;
    if (a.edges.empty()) return true;

    // All T-edge index differences must be the same constant.
    // All I-edge index differences must be the same constant.
    bool seen_t = false, seen_i = false;
    int t_diff = 0, i_diff = 0;

    for (std::size_t i = 0; i < a.edges.size(); ++i) {
        int diff = ((b.edges[i].label.index - a.edges[i].label.index) % 12 + 12) % 12;
        if (a.edges[i].label.kind == TILabel::Kind::T) {
            if (!seen_t) {
                t_diff = diff;
                seen_t = true;
            } else if (diff != t_diff) {
                return false;
            }
        } else {
            if (!seen_i) {
                i_diff = diff;
                seen_i = true;
            } else if (diff != i_diff) {
                return false;
            }
        }
    }
    return true;
}

Result<KNet> knet_from_nodes(
    std::span<const PitchClass> nodes,
    std::span<const KNetEdgeSpec> edge_specs
) {
    KNet net;
    net.nodes.assign(nodes.begin(), nodes.end());
    net.edges.reserve(edge_specs.size());

    for (const auto& spec : edge_specs) {
        if (spec.from >= nodes.size() || spec.to >= nodes.size())
            return std::unexpected(ErrorCode::InvalidKNetEdgeIndex);

        KNetEdge edge;
        edge.from = spec.from;
        edge.to = spec.to;

        PitchClass src = nodes[spec.from];
        PitchClass dst = nodes[spec.to];

        if (spec.kind == TILabel::Kind::T) {
            // T_n(src) = dst → n = (dst - src) mod 12
            int n = ((static_cast<int>(dst) - static_cast<int>(src)) % 12 + 12) % 12;
            edge.label = TILabel::transposition(n);
        } else {
            // I_n(src) = dst → n - src = dst → n = (dst + src) mod 12
            int n = (static_cast<int>(dst) + static_cast<int>(src)) % 12;
            edge.label = TILabel::inversion(n);
        }

        net.edges.push_back(edge);
    }

    return net;
}

}  // namespace Sunny::Core
