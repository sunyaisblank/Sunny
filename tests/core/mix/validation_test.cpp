/**
 * @file validation_test.cpp
 * @brief Unit tests for Mix IR validation
 *
 *
 * Coverage: X0-X11 structural rules, I1-I5 intent rules,
 *           aux send targets, nesting depth
 */

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/core/mix/validation.hpp>
#include <sunny/core/mix/workflows.hpp>

using namespace sunny::core;

namespace {

// Helper: create a minimal valid MixGraph with N channels
MixGraph make_graph(int n_channels) {
    std::vector<PartId> parts;
    for (int i = 1; i <= n_channels; ++i)
        parts.push_back(PartId{static_cast<std::uint64_t>(i)});
    return create_mix_graph(MixGraphId{1}, parts);
}

bool has_rule(const std::vector<Diagnostic>& diags, const std::string& rule) {
    for (const auto& d : diags)
        if (d.rule == rule) return true;
    return false;
}

} // anonymous namespace

// =============================================================================
// X1: exact Score Part-to-ChannelStrip correspondence
// =============================================================================

TEST_CASE("X1 requires an exact one-to-one Part-to-channel correspondence",
          "[mix-ir][validation][correspondence]") {
    Score score;
    for (std::uint64_t id : {1, 2}) {
        Part part;
        part.id = PartId{id};
        part.definition.name = "Part " + std::to_string(id);
        score.parts.push_back(std::move(part));
    }

    auto graph = make_graph(1);
    auto duplicate = graph.channels.front();
    duplicate.id = ChannelStripId{99};
    graph.channels.push_back(duplicate);
    auto unknown = duplicate;
    unknown.id = ChannelStripId{100};
    unknown.part_id = PartId{999};
    graph.channels.push_back(unknown);

    const auto diags = validate_mix_correspondence(score, graph);
    REQUIRE(diags.size() == 3);
    CHECK(diags[0].error_code == ErrorCode::UnknownChannelPart);
    CHECK(diags[1].error_code == ErrorCode::DuplicateChannelForPart);
    CHECK(diags[2].error_code == ErrorCode::MissingChannel);
}

// =============================================================================
// X2: DAG acyclicity
// =============================================================================

TEST_CASE("X2 passes for acyclic graph", "[mix-ir][validation]") {
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

TEST_CASE("X2 detects group bus cycle", "[mix-ir][validation]") {
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

TEST_CASE("X9 rejects invalid relative fader dependency graphs",
          "[mix-ir][validation][relative-level]") {
    auto graph = make_graph(2);
    graph.channels[0].fader.relative_level = RelativeLevel{
        LevelReference{LevelReferenceType::Channel, -14.0f, graph.channels[1].id, {}, {}}, 0.0f};
    graph.channels[1].fader.relative_level = RelativeLevel{
        LevelReference{LevelReferenceType::Channel, -14.0f, graph.channels[0].id, {}, {}}, 0.0f};
    CHECK(has_rule(validate_mix(graph), "X9"));

    graph.channels[1].fader.relative_level.reset();
    graph.channels[0].fader.relative_level->reference.channel_id = ChannelStripId{999};
    CHECK(has_rule(validate_mix(graph), "X9"));

    graph.channels[0].fader.relative_level.reset();
    graph.channels[0].fader.level_db = 12.01f;
    CHECK(has_rule(validate_mix(graph), "X9"));
}

// =============================================================================
// X3: Channel reaches master
// =============================================================================

TEST_CASE("X3 passes for ungrouped channels", "[mix-ir][validation]") {
    auto graph = make_graph(2);
    auto diags = validate_mix(graph);
    CHECK_FALSE(has_rule(diags, "X3"));
}

TEST_CASE("X3 passes for channels routed through group to master", "[mix-ir][validation]") {
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

TEST_CASE("X3 detects channel that cannot reach master", "[mix-ir][validation]") {
    auto graph = make_graph(1);

    // Assign channel to a group that doesn't exist in the graph's group_buses
    // (the group itself has output to another non-existent group)
    GroupBus orphan;
    orphan.id = GroupBusId{999};
    orphan.name = "Orphan";
    orphan.output.type = GroupOutputType::Group;
    orphan.output.parent_group_id = GroupBusId{888}; // does not exist
    graph.group_buses.push_back(orphan);

    graph.channels[0].group_assignment = orphan.id;

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "X3"));
}

// =============================================================================
// X10: redundant group membership/routing representations agree
// =============================================================================

TEST_CASE("X10 requires channel assignments and group member lists to be exact mirrors",
          "[mix-ir][validation][routing]") {
    auto graph = make_graph(2);
    GroupBus group;
    group.id = GroupBusId{10};
    group.name = "Group";
    group.member_channels = {graph.channels[0].id};
    graph.group_buses.push_back(group);

    auto diagnostics = validate_mix(graph);
    CHECK(has_rule(diagnostics, "X10"));
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto& diagnostic) {
        return diagnostic.error_code == ErrorCode::InconsistentRoutingMembership;
    }));

    graph.channels[0].group_assignment = group.id;
    diagnostics = validate_mix(graph);
    CHECK_FALSE(has_rule(diagnostics, "X10"));

    graph.group_buses[0].member_channels.clear();
    diagnostics = validate_mix(graph);
    CHECK(has_rule(diagnostics, "X10"));

    graph.group_buses[0].member_channels = {graph.channels[0].id, graph.channels[0].id};
    diagnostics = validate_mix(graph);
    CHECK(has_rule(diagnostics, "X10"));
}

TEST_CASE("X10 requires child output and parent member lists to be exact mirrors",
          "[mix-ir][validation][routing]") {
    auto graph = make_graph(1);
    GroupBus parent;
    parent.id = GroupBusId{10};
    parent.name = "Parent";
    GroupBus child;
    child.id = GroupBusId{11};
    child.name = "Child";
    child.output.type = GroupOutputType::Group;
    child.output.parent_group_id = parent.id;
    graph.group_buses = {parent, child};

    auto diagnostics = validate_mix(graph);
    CHECK(has_rule(diagnostics, "X10"));

    graph.group_buses[0].member_groups = {child.id};
    diagnostics = validate_mix(graph);
    CHECK_FALSE(has_rule(diagnostics, "X10"));

    graph.group_buses[1].output.parent_group_id = GroupBusId{999};
    diagnostics = validate_mix(graph);
    CHECK(has_rule(diagnostics, "X10"));
}

// =============================================================================
// X3b: Nesting depth
// =============================================================================

TEST_CASE("X3b detects excessive nesting depth", "[mix-ir][validation]") {
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

TEST_CASE("X4 warns for channel with no insert chain", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "X4"));
}

TEST_CASE("X4 does not warn when channel has effects", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    graph.channels[0].insert_chain.effects.push_back({MixEffectId{1}, MixEQ{}, true});
    auto diags = validate_mix(graph);
    CHECK_FALSE(has_rule(diags, "X4"));
}

// =============================================================================
// X5: Silent but not muted
// =============================================================================

TEST_CASE("X5 warns for very low fader without mute", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    graph.channels[0].fader.level_db = -100.0f;
    graph.channels[0].mute = false;

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "X5"));
}

TEST_CASE("X5 does not warn when muted", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    graph.channels[0].fader.level_db = -100.0f;
    graph.channels[0].mute = true;

    auto diags = validate_mix(graph);
    CHECK_FALSE(has_rule(diags, "X5"));
}

TEST_CASE("X5 evaluates the resolved relative fader", "[mix-ir][validation][relative-level]") {
    auto graph = make_graph(2);
    graph.channels[1].fader.level_db = -100.0f;
    graph.channels[1].mute = true;
    graph.channels[0].fader.relative_level = RelativeLevel{
        LevelReference{LevelReferenceType::Channel, -14.0f, graph.channels[1].id, {}, {}}, 0.0f};

    CHECK(has_rule(validate_mix(graph), "X5"));
}

// =============================================================================
// X6: Sidechain references
// =============================================================================

TEST_CASE("X6 detects invalid sidechain channel reference", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    MixCompressor comp;
    comp.sidechain.source = SidechainSourceType::ExternalChannel;
    comp.sidechain.channel_id = ChannelStripId{999}; // does not exist

    graph.channels[0].insert_chain.effects.push_back({MixEffectId{1}, comp, true});

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "X6"));
}

TEST_CASE("X6 passes for valid sidechain reference", "[mix-ir][validation]") {
    auto graph = make_graph(2);
    MixCompressor comp;
    comp.sidechain.source = SidechainSourceType::ExternalChannel;
    comp.sidechain.channel_id = graph.channels[1].id;

    graph.channels[0].insert_chain.effects.push_back({MixEffectId{1}, comp, true});

    auto diags = validate_mix(graph);
    CHECK_FALSE(has_rule(diags, "X6"));
}

TEST_CASE("X6 traverses aux and nested multiband sidechains", "[mix-ir][validation][effects]") {
    auto graph = make_graph(1);
    MixCompressor nested;
    nested.sidechain.source = SidechainSourceType::ExternalBus;
    nested.sidechain.bus_id = GroupBusId{999};
    MultibandDynamicsBand band;
    band.compressor = nested;
    MixMultibandDynamics multiband;
    multiband.bands.push_back(band);
    AuxBus aux;
    aux.id = AuxBusId{4};
    aux.effect_chain.effects.emplace_back(MixEffectId{4}, multiband, true);
    graph.aux_buses.push_back(std::move(aux));

    CHECK(has_rule(validate_mix(graph), "X6"));
}

TEST_CASE("X6 detects send to non-existent aux bus", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    graph.channels[0].sends.push_back({AuxBusId{999}, -6.0f, false, true});

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "X6"));
}

TEST_CASE("X11 rejects duplicate and non-finite channel or group sends", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    AuxBus aux;
    aux.id = AuxBusId{10};
    graph.aux_buses.push_back(aux);
    graph.channels[0].sends = {{AuxBusId{10}, -6.0f, false, true},
                               {AuxBusId{10}, -3.0f, false, false}};
    GroupBus group;
    group.id = GroupBusId{20};
    group.sends.push_back({AuxBusId{10}, std::numeric_limits<float>::infinity(), false, true});
    graph.group_buses.push_back(group);

    const auto diagnostics = validate_mix(graph);
    CHECK(std::count_if(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
              return diagnostic.rule == "X11" && diagnostic.error_code == ErrorCode::InvalidAuxSend;
          }) == 2);
    CHECK_FALSE(is_mix_valid(graph));

    graph.channels[0].sends.erase(graph.channels[0].sends.begin() + 1);
    graph.group_buses[0].sends[0].level_db = -12.0f;
    CHECK_FALSE(has_rule(validate_mix(graph), "X11"));
}

TEST_CASE("X6 validates GroupBus send targets", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    GroupBus group;
    group.id = GroupBusId{20};
    group.sends.push_back({AuxBusId{999}, -6.0f, false, true});
    graph.group_buses.push_back(group);

    CHECK(has_rule(validate_mix(graph), "X6"));
}

TEST_CASE("X7 rejects invalid delay and reverb parameter domains",
          "[mix-ir][validation][effects]") {
    auto graph = make_graph(1);
    MixDelay delay;
    delay.feedback = 1.1f;
    graph.channels[0].insert_chain.effects.push_back({MixEffectId{1}, delay, true});

    MixReverb reverb;
    reverb.algorithm = MixReverbAlgorithm::Convolution;
    graph.master_bus.insert_chain.effects.push_back({MixEffectId{2}, reverb, true});

    auto diagnostics = validate_mix(graph);
    CHECK(std::count_if(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
              return diagnostic.rule == "X7" && diagnostic.severity == ValidationSeverity::Error;
          }) == 2);
    CHECK_FALSE(is_mix_valid(graph));
}

TEST_CASE("X7 covers every Mix effect variant's tractable numeric domain",
          "[mix-ir][validation][effects]") {
    auto graph = make_graph(1);
    MixEQ eq;
    eq.bands.push_back({1000.0f, 0.0f, 0.0f, MixEQBandType::Peak, std::nullopt});
    graph.master_bus.insert_chain.effects.emplace_back(MixEffectId{10}, eq, true);

    MixCompressor compressor;
    compressor.stereo_link = 2.0f;
    graph.master_bus.insert_chain.effects.emplace_back(MixEffectId{11}, compressor, true);
    MixGate gate;
    gate.range = 1.0f;
    graph.master_bus.insert_chain.effects.emplace_back(MixEffectId{12}, gate, true);
    MixLimiter limiter;
    limiter.release = 0.0f;
    graph.master_bus.insert_chain.effects.emplace_back(MixEffectId{13}, limiter, true);
    MixMultibandDynamics multiband;
    multiband.crossover_frequencies = {200.0f};
    multiband.bands = {MultibandDynamicsBand{}};
    graph.master_bus.insert_chain.effects.emplace_back(MixEffectId{14}, multiband, true);
    MixSaturation saturation;
    saturation.drive = 2.0f;
    graph.master_bus.insert_chain.effects.emplace_back(MixEffectId{15}, saturation, true);
    MixStereoProcessor stereo;
    stereo.width = -1.0f;
    graph.master_bus.insert_chain.effects.emplace_back(MixEffectId{16}, stereo, true);

    const auto diagnostics = validate_mix(graph);
    CHECK(std::count_if(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
              return diagnostic.rule == "X7" && diagnostic.severity == ValidationSeverity::Error;
          }) == 7);
    CHECK_FALSE(is_mix_valid(graph));
}

// =============================================================================
// I1, I2: Intent annotations
// =============================================================================

TEST_CASE("I1 reports missing channel intent", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "I1"));
}

TEST_CASE("I1 does not report when intent is present", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    graph.channels[0].intent = ChannelIntent{};
    auto diags = validate_mix(graph);
    CHECK_FALSE(has_rule(diags, "I1"));
}

TEST_CASE("I2 reports missing group intent", "[mix-ir][validation]") {
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

TEST_CASE("I3 warns when Lead channel is below average", "[mix-ir][validation]") {
    auto graph = make_graph(3);
    graph.channels[0].fader.level_db = 0.0f;
    graph.channels[1].fader.level_db = 0.0f;
    graph.channels[2].fader.level_db = -12.0f;
    graph.channels[2].intent = ChannelIntent{};
    graph.channels[2].intent->role_in_mix = MixRole::Lead;

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "I3"));
}

TEST_CASE("I3 does not warn when Lead is above average", "[mix-ir][validation]") {
    auto graph = make_graph(2);
    graph.channels[0].fader.level_db = -6.0f;
    graph.channels[1].fader.level_db = 3.0f;
    graph.channels[1].intent = ChannelIntent{};
    graph.channels[1].intent->role_in_mix = MixRole::Lead;

    auto diags = validate_mix(graph);
    CHECK_FALSE(has_rule(diags, "I3"));
}

TEST_CASE("I3 evaluates the resolved relative fader instead of its fallback",
          "[mix-ir][validation][relative-level]") {
    auto graph = make_graph(3);
    ChannelIntent lead;
    lead.role_in_mix = MixRole::Lead;
    graph.channels[0].intent = lead;
    graph.channels[0].fader.relative_level = RelativeLevel{
        LevelReference{
            LevelReferenceType::Channel, -14.0f, graph.channels[1].id, "20 dB below", {}},
        -20.0f};

    CHECK(has_rule(validate_mix(graph), "I3"));
}

// =============================================================================
// I4: Foundation with HPF removing sub-bass
// =============================================================================

TEST_CASE("I4 warns for Foundation channel with HPF above 60 Hz", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    graph.channels[0].intent = ChannelIntent{};
    graph.channels[0].intent->role_in_mix = MixRole::Foundation;

    MixEQ eq;
    eq.bands.push_back({100.0f, 0.0f, 0.7f, MixEQBandType::LowCut, std::nullopt});
    graph.channels[0].insert_chain.effects.push_back({MixEffectId{1}, eq, true});

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "I4"));
}

// =============================================================================
// I5: Flat depth staging
// =============================================================================

TEST_CASE("I5 reports flat depth staging", "[mix-ir][validation]") {
    auto graph = make_graph(3);
    for (auto& ch : graph.channels) {
        ch.intent = ChannelIntent{};
        ch.intent->depth_position = DepthPosition::Mid;
    }

    auto diags = validate_mix(graph);
    CHECK(has_rule(diags, "I5"));
}

TEST_CASE("I5 does not report varied depth", "[mix-ir][validation]") {
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

TEST_CASE("is_mix_valid returns true for clean graph", "[mix-ir][validation]") {
    auto graph = make_graph(2);
    // Add effects to suppress X4 warnings (which are not errors)
    std::uint64_t effect_id = 1;
    for (auto& ch : graph.channels)
        ch.insert_chain.effects.push_back({MixEffectId{effect_id++}, MixEQ{}, true});

    CHECK(is_mix_valid(graph));
}

TEST_CASE("X0 rejects duplicate MixEffect identities across chains",
          "[mix-ir][validation][device-parameter]") {
    auto graph = make_graph(1);
    graph.channels[0].insert_chain.effects.emplace_back(MixEffectId{7}, MixEQ{}, true);
    graph.master_bus.insert_chain.effects.emplace_back(MixEffectId{7}, MixLimiter{}, true);

    const auto diagnostics = validate_mix(graph);
    CHECK(has_rule(diagnostics, "X0"));
    CHECK_FALSE(is_mix_valid(graph));
}

TEST_CASE("X8 rejects unresolved, invalid, and aliasing mappings",
          "[mix-ir][validation][device-parameter]") {
    auto graph = make_graph(1);
    MixEffect effect{MixEffectId{8}, MixCompressor{}, true};
    MixDeviceParameter valid;
    valid.parameter_name = "Threshold";
    valid.source_min = -60.0f;
    valid.source_max = 0.0f;
    effect.parameter_map["threshold"] = valid;

    SECTION("unresolved source") {
        effect.parameter_map["missing"] = valid;
    }
    SECTION("invalid mapping domain") {
        effect.parameter_map["threshold"].source_max = -60.0f;
    }
    SECTION("target-name alias") {
        effect.parameter_map["ratio"] = valid;
    }
    graph.channels[0].insert_chain.effects.push_back(std::move(effect));

    const auto diagnostics = validate_mix(graph);
    CHECK(has_rule(diagnostics, "X8"));
    CHECK_FALSE(is_mix_valid(graph));
}

TEST_CASE("is_mix_valid returns false with errors", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    // Create an invalid sidechain reference (X6 error)
    MixCompressor comp;
    comp.sidechain.source = SidechainSourceType::ExternalChannel;
    comp.sidechain.channel_id = ChannelStripId{999};
    graph.channels[0].insert_chain.effects.push_back({MixEffectId{1}, comp, true});

    CHECK_FALSE(is_mix_valid(graph));
}

// =============================================================================
// Diagnostic sort order
// =============================================================================

TEST_CASE("Mix diagnostics are sorted by severity", "[mix-ir][validation]") {
    auto graph = make_graph(1);
    // This will produce X4 (Warning), I1 (Info), possibly others
    auto diags = validate_mix(graph);
    REQUIRE(diags.size() >= 2);

    for (std::size_t i = 1; i < diags.size(); ++i) {
        CHECK(static_cast<int>(diags[i - 1].severity) <= static_cast<int>(diags[i].severity));
    }
}
