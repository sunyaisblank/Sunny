/**
 * @file GIKN001A.h
 * @brief Klumpenhouwer Networks
 *
 * Component: GIKN001A
 * Domain: GI (Generalised Interval) | Category: KN (K-Net)
 *
 * Formal Spec §11.5: A K-net is a directed graph where nodes are
 * pitch classes and edges are labelled with T/I group transformations.
 *
 * Well-formedness: for every cycle, the composition of edge labels
 * equals the identity.
 *
 * Isography: two K-nets share graph structure with compatible labels.
 * Strong isography: labels related by uniform transposition.
 *
 * Invariants:
 * - A well-formed K-net satisfies cycle composition = identity
 * - apply_edge(src, T_n) = (src + n) mod 12
 * - apply_edge(src, I_n) = (n - src) mod 12
 * - compose(T_a, T_b) = T_{a+b}
 * - compose(I_a, I_b) = T_{a-b}
 * - compose(T_a, I_b) = I_{a+b}
 * - compose(I_a, T_b) = I_{a-b}
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Pitch/PTPC001A.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// T/I Group Edge Labels
// =============================================================================

/**
 * @brief Edge label in the T/I group (dihedral group D_12)
 *
 * Two kinds: T_n (transposition by n) and I_n (inversion mapping x → (n-x) mod 12).
 */
struct TILabel {
    enum class Kind : std::uint8_t { T, I };
    Kind kind;
    int index;  ///< n in T_n or I_n, normalised to [0, 11]

    [[nodiscard]] static constexpr TILabel transposition(int n) noexcept {
        return {Kind::T, ((n % 12) + 12) % 12};
    }

    [[nodiscard]] static constexpr TILabel inversion(int n) noexcept {
        return {Kind::I, ((n % 12) + 12) % 12};
    }

    [[nodiscard]] static constexpr TILabel identity() noexcept {
        return {Kind::T, 0};
    }

    [[nodiscard]] constexpr bool is_identity() const noexcept {
        return kind == Kind::T && index == 0;
    }

    [[nodiscard]] constexpr bool operator==(const TILabel& o) const noexcept {
        return kind == o.kind && index == o.index;
    }

    [[nodiscard]] constexpr bool operator!=(const TILabel& o) const noexcept {
        return !(*this == o);
    }

    /**
     * @brief Apply this transformation to a pitch class
     *
     * T_n(x) = (x + n) mod 12
     * I_n(x) = (n - x) mod 12
     */
    [[nodiscard]] constexpr PitchClass apply(PitchClass pc) const noexcept {
        if (kind == Kind::T) {
            return static_cast<PitchClass>(
                (static_cast<int>(pc) + index) % 12);
        }
        return static_cast<PitchClass>(
            ((index - static_cast<int>(pc)) % 12 + 12) % 12);
    }

    /**
     * @brief Compose this label with another: this ∘ other
     *
     * T_a ∘ T_b = T_{a+b}
     * I_a ∘ T_b = I_{a-b}
     * T_a ∘ I_b = I_{a+b}
     * I_a ∘ I_b = T_{a-b}
     */
    [[nodiscard]] constexpr TILabel compose(const TILabel& other) const noexcept {
        if (kind == Kind::T && other.kind == Kind::T) {
            return transposition(index + other.index);
        }
        if (kind == Kind::T && other.kind == Kind::I) {
            return inversion(index + other.index);
        }
        if (kind == Kind::I && other.kind == Kind::T) {
            return inversion(index - other.index);
        }
        // I ∘ I = T
        return transposition(index - other.index);
    }

    /**
     * @brief String representation: "T0"–"T11" or "I0"–"I11"
     */
    [[nodiscard]] std::string to_string() const {
        return std::string(kind == Kind::T ? "T" : "I") + std::to_string(index);
    }
};

// =============================================================================
// K-Net Graph
// =============================================================================

/**
 * @brief Directed edge in a K-net
 */
struct KNetEdge {
    std::size_t from;    ///< Source node index
    std::size_t to;      ///< Target node index
    TILabel label;       ///< T/I transformation
};

/**
 * @brief Klumpenhouwer Network
 *
 * A directed graph with pitch-class nodes and T/I-labelled edges.
 */
struct KNet {
    std::vector<PitchClass> nodes;
    std::vector<KNetEdge> edges;
};

/**
 * @brief Check edge consistency: label maps source to target
 *
 * Returns true if, for every edge (u, v, L), L(nodes[u]) == nodes[v].
 */
[[nodiscard]] bool knet_edges_consistent(const KNet& net);

/**
 * @brief Check cycle well-formedness
 *
 * Returns true if for every simple cycle in the graph, the composition
 * of edge labels around the cycle equals the identity.
 *
 * For small K-nets (typical analytical use: 3–6 nodes), this checks
 * all cycles found by DFS.
 */
[[nodiscard]] bool knet_well_formed(const KNet& net);

/**
 * @brief Check if two K-nets are isographic
 *
 * Isographic: same graph structure (node count, edge topology)
 * with T-edges mapping to T-edges and I-edges mapping to I-edges.
 * The specific indices may differ.
 */
[[nodiscard]] bool knet_isographic(const KNet& a, const KNet& b);

/**
 * @brief Check if two K-nets are strongly isographic
 *
 * Strongly isographic: isographic, and all corresponding T-labels
 * differ by the same constant, and all corresponding I-labels differ
 * by the same constant. Equivalently: one K-net can be obtained from
 * the other by applying a uniform <T_n> or <I_n> to all nodes.
 */
[[nodiscard]] bool knet_strongly_isographic(const KNet& a, const KNet& b);

/**
 * @brief Build a K-net from pitch classes and edges, inferring labels
 *
 * Given node pitch classes and directed edge pairs with specified
 * T/I kind, computes the required index for each edge label.
 *
 * @param nodes Pitch class values for each node
 * @param edge_specs (from, to, kind) triples
 * @return K-net with computed labels
 */
struct KNetEdgeSpec {
    std::size_t from;
    std::size_t to;
    TILabel::Kind kind;  ///< Whether this edge is T or I
};

[[nodiscard]] KNet knet_from_nodes(
    std::span<const PitchClass> nodes,
    std::span<const KNetEdgeSpec> edge_specs);

}  // namespace Sunny::Core
