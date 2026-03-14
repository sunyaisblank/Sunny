/**
 * @file TSMI003A.cpp
 * @brief Unit tests for Mix IR validation (MIVD001A)
 *
 * Component: TSMI003A
 * Domain: TS (Test) | Category: MI (Mix IR)
 *
 * Tests: MIVD001A
 * Coverage: X1-X6 structural rules, I1-I5 intent rules,
 *           aux send targets, nesting depth
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Mix/MIVD001A.h"
#include "Mix/MIWF001A.h"

using namespace Sunny::Core;

namespace {

// Helper: create a minimal valid MixGraph with N channels
MixGraph make_graph(int n_channels) {
    std::vector<PartId> parts;
    for (int i = 1; i <= n_channels; ++i)
        parts.push_back(PartId{static_cast<std::uint64_t>(i)});
    return wf_create_mix_graph(MixGraphId{1}, parts);
}

bool has_rule(const std::vector<Diagnostic>& diags, const std::string& rule) {
    for (const auto& d : diags)
        if (d.rule == rule) return true;
    return false;
}

bool has_error(const std::vector<Diagnostic>& diags) {
    for (const auto& d : diags)
        if (d.severity == ValidationSeverity::Error) return true;
    return false;
}

}  // anonymous namespace

// =============================================================================
// X2: DAG acyclicity
// =============================================================================

TEST_CASE("MIVD001A: X2 passes for acyclic graph", "[mix-ir][validation]") {
    auto graph = make_graph(2);

    // Create two groups: child → parent → master
    GroupBus parent;
    parent.id = GroupBusId{100};
    parent.name = "All";
    parent.output.type = GroupOutputType::Master;

    GroupBus child;
    child.id = GroupBusId{101};
    child.name = "Subset";
    child.output.type = GroupOutputType::Group;
    child.output.parent_group_id = parent.id;

    parent.member_groups.push_back(child.id);
    graph.group_buses = {parent, child};

    auto diags = validate_mix(graph);
    CHECK_FALSE(has_rule(diags, "X2"));
}

TEST_CASE("MIVD001A: X2 detects group bus cycle", "[mix-ir][validation]") {
    auto graph = make_graph(1);

    // Create a cycle: A → B → A
    GroupBus a;
    a.id = GroupBusId{1};
    a.name = "A";
    a.member_groups.push_back(GroupBusId{2});
    a.output.type = GroupOutputType::Group;
    a.output.parent_group_id = GroupBusId{2};

    GroupBus b;
    b.id = GroupBusId{2};
    b.name = "B";
    b.member_groups.push_back(GroupBusId{1});
    b.output.type = GroupOutputType::Group;
    b.output.parent_group_id = GroupBusId{1};

    graph.group_buses = {a, b};
    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "X2"));
}

// =============================================================================
// X3: Channel reaches master
// =============================================================================

TEST_CASE("MIVD001A: X3 passes for ungrouped channels", "[mix-ir][validation]") {
    auto graph = make_graph(2);
    auto diags = validate_mix(graph);
    CHECK_FALSE(has_rule(diags, "X3"));
}

TEST_CASE("MIVD001A: X3 passes for channels routed through group to master", "[mix-ir][validation]") {
    auto graph = make_graph(2);

    GroupBus grp;
    grp.id = GroupBusId{10};
    grp.name = "Group";
    grp.member_channels = {graph.channels[0].id, graph.channels[1].id};
    grp.output.type = GroupOutputType::Master;
    graph.group_buses.push_back(grp);

    graph.channels[0].group_assignment = grp.id;
    graph.channels[1].group_assignment = grp.id;

    auto diags = validate_mix(graph);
    CHECK_FALSE(has_rule(diags, "X3"));
}

TEST_CASE("MIVD001A: X3 detects channel that cannot reach master", "[mix-ir][validation]") {
    auto graph = make_graph(1);

    // Assign channel to a group that doesn't exist in the graph's group_buses
    // (the group itself has output to another non-existent group)
    GroupBus orphan;
    orphan.id = GroupBusId{999};
    orphan.name = "Orphan";
    orphan.output.type = GroupOutputType::Group;
    orphan.output.parent_group_id = GroupBusId{888};  // does not exist
    graph.group_buses.push_back(orphan);

    graph.channels[0].group_assignment = orphan.id;

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "X3"));
}

// =============================================================================
// X3b: Nesting depth
// =============================================================================

TEST_CASE("MIVD001A: X3b detects excessive nesting depth", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    graph.max_group_nesting_depth = 1;

    // Create chain: g1 → g2 → g3 → master (depth 2 for g1, exceeds limit of 1)
    GroupBus g3;
    g3.id = GroupBusId{3};
    g3.name = "G3";
    g3.output.type = GroupOutputType::Master;

    GroupBus g2;
    g2.id = GroupBusId{2};
    g2.name = "G2";
    g2.output.type = GroupOutputType::Group;
    g2.output.parent_group_id = g3.id;

    GroupBus g1;
    g1.id = GroupBusId{1};
    g1.name = "G1";
    g1.output.type = GroupOutputType::Group;
    g1.output.parent_group_id = g2.id;

    graph.group_buses = {g1, g2, g3};

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "X3b"));
}

// =============================================================================
// X4: No insert processing (warning)
// =============================================================================

TEST_CASE("MIVD001A: X4 warns for channel with no insert chain", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "X4"));
}

TEST_CASE("MIVD001A: X4 does not warn when channel has effects", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    graph.channels[0].insert_chain.effects.push_back({
        MixEffectId{1}, MixEQ{}, true
    });
    auto diags = validate_mix(graph);
    CHECK_FALSE(has_rule(diags, "X4"));
}

// =============================================================================
// X5: Silent but not muted
// =============================================================================

TEST_CASE("MIVD001A: X5 warns for very low fader without mute", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    graph.channels[0].fader.level_db = -100.0f;
    graph.channels[0].mute = false;

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "X5"));
}

TEST_CASE("MIVD001A: X5 does not warn when muted", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    graph.channels[0].fader.level_db = -100.0f;
    graph.channels[0].mute = true;

    auto diags = validate_mix(graph);
    CHECK_FALSE(has_rule(diags, "X5"));
}

// =============================================================================
// X6: Sidechain references
// =============================================================================

TEST_CASE("MIVD001A: X6 detects invalid sidechain channel reference", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    MixCompressor comp;
    comp.sidechain.source = SidechainSourceType::ExternalChannel;
    comp.sidechain.channel_id = ChannelStripId{999};  // does not exist

    graph.channels[0].insert_chain.effects.push_back({
        MixEffectId{1}, comp, true
    });

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "X6"));
}

TEST_CASE("MIVD001A: X6 passes for valid sidechain reference", "[mix-ir][validation]") {
    auto graph = make_graph(2);
    MixCompressor comp;
    comp.sidechain.source = SidechainSourceType::ExternalChannel;
    comp.sidechain.channel_id = graph.channels[1].id;

    graph.channels[0].insert_chain.effects.push_back({
        MixEffectId{1}, comp, true
    });

    auto diags = validate_mix(graph);
    CHECK_FALSE(has_rule(diags, "X6"));
}

TEST_CASE("MIVD001A: X6 detects send to non-existent aux bus", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    graph.channels[0].sends.push_back({AuxBusId{999}, -6.0f, false, true});

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "X6"));
}

// =============================================================================
// I1, I2: Intent annotations
// =============================================================================

TEST_CASE("MIVD001A: I1 reports missing channel intent", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "I1"));
}

TEST_CASE("MIVD001A: I1 does not report when intent is present", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    graph.channels[0].intent = ChannelIntent{};
    auto diags = validate_mix(graph);
    CHECK_FALSE(has_rule(diags, "I1"));
}

TEST_CASE("MIVD001A: I2 reports missing group intent", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    GroupBus grp;
    grp.id = GroupBusId{10};
    grp.name = "Test";
    graph.group_buses.push_back(grp);

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "I2"));
}

// =============================================================================
// I3: Lead role below average level
// =============================================================================

TEST_CASE("MIVD001A: I3 warns when Lead channel is below average", "[mix-ir][validation]") {
    auto graph = make_graph(3);
    graph.channels[0].fader.level_db = 0.0f;
    graph.channels[1].fader.level_db = 0.0f;
    graph.channels[2].fader.level_db = -12.0f;
    graph.channels[2].intent = ChannelIntent{};
    graph.channels[2].intent->role_in_mix = MixRole::Lead;

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "I3"));
}

TEST_CASE("MIVD001A: I3 does not warn when Lead is above average", "[mix-ir][validation]") {
    auto graph = make_graph(2);
    graph.channels[0].fader.level_db = -6.0f;
    graph.channels[1].fader.level_db = 3.0f;
    graph.channels[1].intent = ChannelIntent{};
    graph.channels[1].intent->role_in_mix = MixRole::Lead;

    auto diags = validate_mix(graph);
    CHECK_FALSE(has_rule(diags, "I3"));
}

// =============================================================================
// I4: Foundation with HPF removing sub-bass
// =============================================================================

TEST_CASE("MIVD001A: I4 warns for Foundation channel with HPF above 60 Hz", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    graph.channels[0].intent = ChannelIntent{};
    graph.channels[0].intent->role_in_mix = MixRole::Foundation;

    MixEQ eq;
    eq.bands.push_back({100.0f, 0.0f, 0.7f, MixEQBandType::LowCut, std::nullopt});
    graph.channels[0].insert_chain.effects.push_back({
        MixEffectId{1}, eq, true
    });

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "I4"));
}

// =============================================================================
// I5: Flat depth staging
// =============================================================================

TEST_CASE("MIVD001A: I5 reports flat depth staging", "[mix-ir][validation]") {
    auto graph = make_graph(3);
    for (auto& ch : graph.channels) {
        ch.intent = ChannelIntent{};
        ch.intent->depth_position = DepthPosition::Mid;
    }

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "I5"));
}

TEST_CASE("MIVD001A: I5 does not report varied depth", "[mix-ir][validation]") {
    auto graph = make_graph(2);
    graph.channels[0].intent = ChannelIntent{};
    graph.channels[0].intent->depth_position = DepthPosition::FrontClose;
    graph.channels[1].intent = ChannelIntent{};
    graph.channels[1].intent->depth_position = DepthPosition::Far;

    auto diags = validate_mix(graph);
    CHECK_FALSE(has_rule(diags, "I5"));
}

// =============================================================================
// is_mix_valid
// =============================================================================

TEST_CASE("MIVD001A: is_mix_valid returns true for clean graph", "[mix-ir][validation]") {
    auto graph = make_graph(2);
    // Add effects to suppress X4 warnings (which are not errors)
    for (auto& ch : graph.channels)
        ch.insert_chain.effects.push_back({MixEffectId{1}, MixEQ{}, true});

    CHECK(is_mix_valid(graph));
}

TEST_CASE("MIVD001A: is_mix_valid returns false with errors", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    // Create an invalid sidechain reference (X6 error)
    MixCompressor comp;
    comp.sidechain.source = SidechainSourceType::ExternalChannel;
    comp.sidechain.channel_id = ChannelStripId{999};
    graph.channels[0].insert_chain.effects.push_back({
        MixEffectId{1}, comp, true
    });

    CHECK_FALSE(is_mix_valid(graph));
}

// =============================================================================
// Diagnostic sort order
// =============================================================================

TEST_CASE("MIVD001A: diagnostics sorted by severity", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    // This will produce X4 (Warning), I1 (Info), possibly others
    auto diags = validate_mix(graph);
    REQUIRE(diags.size() >= 2);

    for (std::size_t i = 1; i < diags.size(); ++i) {
        CHECK(static_cast<int>(diags[i - 1].severity) <=
              static_cast<int>(diags[i].severity));
    }
}
