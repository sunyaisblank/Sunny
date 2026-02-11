/**
 * @file TSKN001A.cpp
 * @brief Unit tests for GIKN001A (Klumpenhouwer Networks)
 *
 * Component: TSKN001A
 * Tests: GIKN001A
 * Validates: Formal Spec §11.5
 */

#include <catch2/catch_test_macros.hpp>

#include "Transform/GIKN001A.h"

using namespace Sunny::Core;

// =============================================================================
// TILabel operations
// =============================================================================

TEST_CASE("GIKN001A: TILabel identity", "[knet][core]") {
    auto id = TILabel::identity();
    REQUIRE(id.kind == TILabel::Kind::T);
    REQUIRE(id.index == 0);
    REQUIRE(id.is_identity());
}

TEST_CASE("GIKN001A: TILabel transposition apply", "[knet][core]") {
    // T_4(C) = E
    auto t4 = TILabel::transposition(4);
    REQUIRE(t4.apply(0) == 4);
    // T_7(G) = D (7+7=14 mod 12 = 2)
    auto t7 = TILabel::transposition(7);
    REQUIRE(t7.apply(7) == 2);
}

TEST_CASE("GIKN001A: TILabel inversion apply", "[knet][core]") {
    // I_0(C) = C, I_0(E) = Ab(8)
    auto i0 = TILabel::inversion(0);
    REQUIRE(i0.apply(0) == 0);
    REQUIRE(i0.apply(4) == 8);   // (0-4) mod 12 = 8
    // I_4(C) = E(4), I_4(E) = C(0)
    auto i4 = TILabel::inversion(4);
    REQUIRE(i4.apply(0) == 4);
    REQUIRE(i4.apply(4) == 0);
}

TEST_CASE("GIKN001A: TILabel composition T∘T", "[knet][core]") {
    // T_3 ∘ T_4 = T_7
    auto result = TILabel::transposition(3).compose(TILabel::transposition(4));
    REQUIRE(result == TILabel::transposition(7));
}

TEST_CASE("GIKN001A: TILabel composition T∘I", "[knet][core]") {
    // T_3 ∘ I_4 = I_7
    auto result = TILabel::transposition(3).compose(TILabel::inversion(4));
    REQUIRE(result == TILabel::inversion(7));
}

TEST_CASE("GIKN001A: TILabel composition I∘T", "[knet][core]") {
    // I_5 ∘ T_3 = I_2
    auto result = TILabel::inversion(5).compose(TILabel::transposition(3));
    REQUIRE(result == TILabel::inversion(2));
}

TEST_CASE("GIKN001A: TILabel composition I∘I", "[knet][core]") {
    // I_5 ∘ I_3 = T_2
    auto result = TILabel::inversion(5).compose(TILabel::inversion(3));
    REQUIRE(result == TILabel::transposition(2));
}

TEST_CASE("GIKN001A: TILabel normalisation", "[knet][core]") {
    REQUIRE(TILabel::transposition(14) == TILabel::transposition(2));
    REQUIRE(TILabel::transposition(-2) == TILabel::transposition(10));
    REQUIRE(TILabel::inversion(-1) == TILabel::inversion(11));
}

TEST_CASE("GIKN001A: TILabel to_string", "[knet][core]") {
    REQUIRE(TILabel::transposition(7).to_string() == "T7");
    REQUIRE(TILabel::inversion(3).to_string() == "I3");
    REQUIRE(TILabel::identity().to_string() == "T0");
}

// =============================================================================
// K-Net construction and consistency
// =============================================================================

TEST_CASE("GIKN001A: knet_from_nodes builds correct labels", "[knet][core]") {
    // Triangle: C(0) → E(4) → G(7) with T edges
    std::vector<PitchClass> nodes = {0, 4, 7};
    std::vector<KNetEdgeSpec> specs = {
        {0, 1, TILabel::Kind::T},  // C→E: T4
        {1, 2, TILabel::Kind::T},  // E→G: T3
        {0, 2, TILabel::Kind::T},  // C→G: T7
    };
    auto net = knet_from_nodes(nodes, specs);
    REQUIRE(net.edges[0].label == TILabel::transposition(4));
    REQUIRE(net.edges[1].label == TILabel::transposition(3));
    REQUIRE(net.edges[2].label == TILabel::transposition(7));
}

TEST_CASE("GIKN001A: knet_from_nodes with inversion edges", "[knet][core]") {
    // C(0) and E(4) related by I_4
    std::vector<PitchClass> nodes = {0, 4};
    std::vector<KNetEdgeSpec> specs = {
        {0, 1, TILabel::Kind::I},  // I_n(0) = 4 → n = 4
    };
    auto net = knet_from_nodes(nodes, specs);
    REQUIRE(net.edges[0].label == TILabel::inversion(4));
    REQUIRE(net.edges[0].label.apply(0) == 4);
}

TEST_CASE("GIKN001A: knet_edges_consistent valid net", "[knet][core]") {
    std::vector<PitchClass> nodes = {0, 4, 7};
    std::vector<KNetEdgeSpec> specs = {
        {0, 1, TILabel::Kind::T},
        {1, 2, TILabel::Kind::T},
    };
    auto net = knet_from_nodes(nodes, specs);
    REQUIRE(knet_edges_consistent(net));
}

TEST_CASE("GIKN001A: knet_edges_consistent detects mismatch", "[knet][core]") {
    KNet net;
    net.nodes = {0, 4};
    net.edges = {{0, 1, TILabel::transposition(5)}};  // T5(0)=5, not 4
    REQUIRE_FALSE(knet_edges_consistent(net));
}

// =============================================================================
// Well-formedness (cycle verification)
// =============================================================================

TEST_CASE("GIKN001A: knet_well_formed triangle all-T", "[knet][core]") {
    // C→E→G→C: T4, T3, T5 — composition: T5∘T3∘T4 = T12 = T0 ✓
    std::vector<PitchClass> nodes = {0, 4, 7};
    std::vector<KNetEdgeSpec> specs = {
        {0, 1, TILabel::Kind::T},  // T4
        {1, 2, TILabel::Kind::T},  // T3
        {2, 0, TILabel::Kind::T},  // T5
    };
    auto net = knet_from_nodes(nodes, specs);
    REQUIRE(knet_well_formed(net));
}

TEST_CASE("GIKN001A: knet_well_formed with inversion edges", "[knet][core]") {
    // Two nodes with I edge and reverse I edge:
    // C(0) →I4→ E(4) →I4→ C(0)
    // I4 ∘ I4 = T0 ✓
    std::vector<PitchClass> nodes = {0, 4};
    std::vector<KNetEdgeSpec> specs = {
        {0, 1, TILabel::Kind::I},  // I4
        {1, 0, TILabel::Kind::I},  // I4(4) = 0, so also I4
    };
    auto net = knet_from_nodes(nodes, specs);
    REQUIRE(knet_well_formed(net));
}

TEST_CASE("GIKN001A: knet_well_formed no cycles is well-formed", "[knet][core]") {
    // Tree: no cycles → vacuously well-formed
    std::vector<PitchClass> nodes = {0, 4, 7};
    std::vector<KNetEdgeSpec> specs = {
        {0, 1, TILabel::Kind::T},
        {0, 2, TILabel::Kind::T},
    };
    auto net = knet_from_nodes(nodes, specs);
    REQUIRE(knet_well_formed(net));
}

// =============================================================================
// Isography
// =============================================================================

TEST_CASE("GIKN001A: knet_isographic same topology", "[knet][core]") {
    // Net A: {C, E, G} with T4, T3
    std::vector<PitchClass> nodes_a = {0, 4, 7};
    std::vector<KNetEdgeSpec> specs = {
        {0, 1, TILabel::Kind::T},
        {1, 2, TILabel::Kind::T},
    };
    auto a = knet_from_nodes(nodes_a, specs);

    // Net B: {D, F#, A} with T4, T3 — same topology, same edge kinds
    std::vector<PitchClass> nodes_b = {2, 6, 9};
    auto b = knet_from_nodes(nodes_b, specs);

    REQUIRE(knet_isographic(a, b));
}

TEST_CASE("GIKN001A: knet_isographic fails on different edge kind", "[knet][core]") {
    std::vector<PitchClass> nodes = {0, 4};
    std::vector<KNetEdgeSpec> specs_t = {{0, 1, TILabel::Kind::T}};
    std::vector<KNetEdgeSpec> specs_i = {{0, 1, TILabel::Kind::I}};

    auto a = knet_from_nodes(nodes, specs_t);
    auto b = knet_from_nodes(nodes, specs_i);
    REQUIRE_FALSE(knet_isographic(a, b));
}

TEST_CASE("GIKN001A: knet_isographic fails on different node count", "[knet][core]") {
    KNet a;
    a.nodes = {0, 4};
    KNet b;
    b.nodes = {0, 4, 7};
    REQUIRE_FALSE(knet_isographic(a, b));
}

// =============================================================================
// Strong isography
// =============================================================================

TEST_CASE("GIKN001A: knet_strongly_isographic transposed K-nets", "[knet][core]") {
    // Net A: C(0)→E(4)→G(7), all T edges
    std::vector<PitchClass> nodes_a = {0, 4, 7};
    std::vector<KNetEdgeSpec> specs = {
        {0, 1, TILabel::Kind::T},
        {1, 2, TILabel::Kind::T},
    };
    auto a = knet_from_nodes(nodes_a, specs);

    // Net B: D(2)→F#(6)→A(9) — transposed up by 2
    // T4 → T4, T3 → T3 (same T-labels, diff = 0)
    std::vector<PitchClass> nodes_b = {2, 6, 9};
    auto b = knet_from_nodes(nodes_b, specs);

    REQUIRE(knet_strongly_isographic(a, b));
}

TEST_CASE("GIKN001A: knet_strongly_isographic fails on non-uniform diff", "[knet][core]") {
    // Net A: edges T4, T3
    KNet a;
    a.nodes = {0, 4, 7};
    a.edges = {
        {0, 1, TILabel::transposition(4)},
        {1, 2, TILabel::transposition(3)},
    };

    // Net B: edges T5, T3 — diff for first edge is 1, diff for second is 0
    KNet b;
    b.nodes = {0, 5, 8};
    b.edges = {
        {0, 1, TILabel::transposition(5)},
        {1, 2, TILabel::transposition(3)},
    };

    REQUIRE_FALSE(knet_strongly_isographic(a, b));
}
