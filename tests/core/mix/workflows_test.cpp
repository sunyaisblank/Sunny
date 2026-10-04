/**
 * @file workflows_test.cpp
 * @brief Unit tests for Mix IR workflow functions
 *
 *
 * Coverage: Graph creation, group bus management, aux bus management,
 *           effect chain mutation, level/spatial control, intent,
 *           loudness targets, automation, reference profiles,
 *           seating templates, validation wrapper
 */

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/mix/validation.hpp>
#include <sunny/core/mix/workflows.hpp>

using namespace sunny::core;

// =============================================================================
// Graph Construction
// =============================================================================

TEST_CASE("create_mix_graph produces one channel per part", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{10}, PartId{20}, PartId{30}});

    CHECK(graph.id.value == 1);
    REQUIRE(graph.channels.size() == 3);
    CHECK(graph.channels[0].part_id.value == 10);
    CHECK(graph.channels[1].part_id.value == 20);
    CHECK(graph.channels[2].part_id.value == 30);
}

TEST_CASE("channels have sequential ids", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}, PartId{2}});
    CHECK(graph.channels[0].id.value == 1);
    CHECK(graph.channels[1].id.value == 2);
}

TEST_CASE("channels default to 0 dB centre pan", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    CHECK(graph.channels[0].fader.level_db == 0.0f);
    CHECK(graph.channels[0].spatial.pan == 0.0f);
}

// =============================================================================
// Group Bus
// =============================================================================

TEST_CASE("create_group_bus adds group and updates channels", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}, PartId{2}});
    auto result = create_group_bus(
        graph, GroupBusId{100}, "Strings", {graph.channels[0].id, graph.channels[1].id});

    REQUIRE(result.has_value());
    REQUIRE(graph.group_buses.size() == 1);
    CHECK(graph.group_buses[0].name == "Strings");
    CHECK(graph.group_buses[0].member_channels.size() == 2);
    REQUIRE(graph.channels[0].group_assignment.has_value());
    CHECK(graph.channels[0].group_assignment->value == 100);
}

TEST_CASE("create_group_bus rejects duplicate id", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)create_group_bus(graph, GroupBusId{100}, "A", {graph.channels[0].id});
    auto result = create_group_bus(graph, GroupBusId{100}, "B", {});
    CHECK_FALSE(result.has_value());
}

TEST_CASE("create_group_bus rejects non-existent members", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto result = create_group_bus(graph, GroupBusId{100}, "X", {ChannelStripId{999}});
    CHECK_FALSE(result.has_value());
}

TEST_CASE("create_group_bus rejects duplicate members transactionally", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    const auto channel_id = graph.channels[0].id;

    auto result = create_group_bus(graph, GroupBusId{100}, "X", {channel_id, channel_id});

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InconsistentRoutingMembership);
    CHECK(graph.group_buses.empty());
    CHECK_FALSE(graph.channels[0].group_assignment.has_value());
}

TEST_CASE("create_group_bus moves existing members without stale reverse edges",
          "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    const auto channel_id = graph.channels[0].id;
    REQUIRE(create_group_bus(graph, GroupBusId{100}, "A", {channel_id}).has_value());

    auto result = create_group_bus(graph, GroupBusId{200}, "B", {channel_id});

    REQUIRE(result.has_value());
    CHECK(graph.group_buses[0].member_channels.empty());
    REQUIRE(graph.group_buses[1].member_channels.size() == 1);
    CHECK(graph.group_buses[1].member_channels[0] == channel_id);
    REQUIRE(graph.channels[0].group_assignment.has_value());
    CHECK(*graph.channels[0].group_assignment == GroupBusId{200});
    CHECK(is_mix_valid(graph));
}

TEST_CASE("assign_channel_to_group moves channel between groups", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)create_group_bus(graph, GroupBusId{100}, "A", {graph.channels[0].id});
    (void)create_group_bus(graph, GroupBusId{200}, "B", {});

    auto result = assign_channel_to_group(graph, graph.channels[0].id, GroupBusId{200});
    REQUIRE(result.has_value());
    CHECK(graph.channels[0].group_assignment->value == 200);
    CHECK(graph.group_buses[0].member_channels.empty());
    CHECK(graph.group_buses[1].member_channels.size() == 1);
}

TEST_CASE("assign_channel_to_group repairs stale and duplicate reverse edges",
          "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    const auto channel_id = graph.channels[0].id;
    REQUIRE(create_group_bus(graph, GroupBusId{100}, "A", {}).has_value());
    REQUIRE(create_group_bus(graph, GroupBusId{200}, "B", {}).has_value());
    graph.group_buses[0].member_channels = {channel_id, channel_id};
    graph.group_buses[1].member_channels = {channel_id};

    auto result = assign_channel_to_group(graph, channel_id, GroupBusId{200});

    REQUIRE(result.has_value());
    CHECK(graph.group_buses[0].member_channels.empty());
    REQUIRE(graph.group_buses[1].member_channels.size() == 1);
    CHECK(graph.group_buses[1].member_channels[0] == channel_id);
    REQUIRE(graph.channels[0].group_assignment.has_value());
    CHECK(*graph.channels[0].group_assignment == GroupBusId{200});
    CHECK(is_mix_valid(graph));
}

TEST_CASE("assign_group_to_group installs one exact mirrored nesting edge",
          "[mix-ir][workflow][routing]") {
    auto graph = create_mix_graph(MixGraphId{1}, {});
    REQUIRE(create_group_bus(graph, GroupBusId{100}, "Child", {}).has_value());
    REQUIRE(create_group_bus(graph, GroupBusId{200}, "Parent", {}).has_value());

    const auto result = assign_group_to_group(graph, GroupBusId{100}, GroupBusId{200});

    REQUIRE(result.has_value());
    CHECK(graph.group_buses[0].output.type == GroupOutputType::Group);
    CHECK(graph.group_buses[0].output.parent_group_id == GroupBusId{200});
    CHECK(graph.group_buses[1].member_groups == std::vector<GroupBusId>{GroupBusId{100}});
    CHECK(is_mix_valid(graph));
}

TEST_CASE("assign_group_to_group moves and repairs stale reverse nesting edges",
          "[mix-ir][workflow][routing]") {
    auto graph = create_mix_graph(MixGraphId{1}, {});
    REQUIRE(create_group_bus(graph, GroupBusId{100}, "Child", {}).has_value());
    REQUIRE(create_group_bus(graph, GroupBusId{200}, "Old", {}).has_value());
    REQUIRE(create_group_bus(graph, GroupBusId{300}, "New", {}).has_value());
    graph.group_buses[1].member_groups = {GroupBusId{100}, GroupBusId{100}};
    graph.group_buses[2].member_groups = {GroupBusId{100}};

    const auto result = assign_group_to_group(graph, GroupBusId{100}, GroupBusId{300});

    REQUIRE(result.has_value());
    CHECK(graph.group_buses[1].member_groups.empty());
    CHECK(graph.group_buses[2].member_groups == std::vector<GroupBusId>{GroupBusId{100}});
    CHECK(graph.group_buses[0].output.parent_group_id == GroupBusId{300});
    CHECK(is_mix_valid(graph));
}

TEST_CASE("assign_group_to_group rejects cycles and excessive depth transactionally",
          "[mix-ir][workflow][routing]") {
    auto graph = create_mix_graph(MixGraphId{1}, {});
    REQUIRE(create_group_bus(graph, GroupBusId{100}, "A", {}).has_value());
    REQUIRE(create_group_bus(graph, GroupBusId{200}, "B", {}).has_value());
    REQUIRE(assign_group_to_group(graph, GroupBusId{100}, GroupBusId{200}).has_value());
    const auto before_cycle = graph;

    auto result = assign_group_to_group(graph, GroupBusId{200}, GroupBusId{100});

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::SignalFlowCycle);
    CHECK(graph.group_buses[0].output.type == before_cycle.group_buses[0].output.type);
    CHECK(graph.group_buses[0].output.parent_group_id ==
          before_cycle.group_buses[0].output.parent_group_id);
    CHECK(graph.group_buses[1].member_groups == before_cycle.group_buses[1].member_groups);

    REQUIRE(route_group_to_master(graph, GroupBusId{100}).has_value());
    graph.max_group_nesting_depth = 0;
    const auto before_depth = graph;
    result = assign_group_to_group(graph, GroupBusId{100}, GroupBusId{200});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::NestingDepthExceeded);
    CHECK(graph.group_buses[0].output.type == before_depth.group_buses[0].output.type);
    CHECK(graph.group_buses[1].member_groups == before_depth.group_buses[1].member_groups);
}

TEST_CASE("route_group_to_master removes all stale parent membership edges",
          "[mix-ir][workflow][routing]") {
    auto graph = create_mix_graph(MixGraphId{1}, {});
    REQUIRE(create_group_bus(graph, GroupBusId{100}, "Child", {}).has_value());
    REQUIRE(create_group_bus(graph, GroupBusId{200}, "Parent", {}).has_value());
    REQUIRE(assign_group_to_group(graph, GroupBusId{100}, GroupBusId{200}).has_value());
    graph.group_buses[1].member_groups.push_back(GroupBusId{100});

    const auto result = route_group_to_master(graph, GroupBusId{100});

    REQUIRE(result.has_value());
    CHECK(graph.group_buses[0].output.type == GroupOutputType::Master);
    CHECK(graph.group_buses[0].output.parent_group_id == GroupBusId{});
    CHECK(graph.group_buses[1].member_groups.empty());
    CHECK(is_mix_valid(graph));
}

// =============================================================================
// Aux Bus
// =============================================================================

TEST_CASE("create_aux_bus adds aux bus", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto result = create_aux_bus(graph, AuxBusId{200}, "Reverb");
    REQUIRE(result.has_value());
    CHECK(graph.aux_buses.size() == 1);
    CHECK(graph.aux_buses[0].name == "Reverb");
}

TEST_CASE("create_aux_bus rejects duplicate id", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)create_aux_bus(graph, AuxBusId{200}, "A");
    auto result = create_aux_bus(graph, AuxBusId{200}, "B");
    CHECK_FALSE(result.has_value());
}

TEST_CASE("set_channel_send configures send", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)create_aux_bus(graph, AuxBusId{200}, "Reverb");

    auto result = set_channel_send(graph, graph.channels[0].id, AuxBusId{200}, -6.0f, true);
    REQUIRE(result.has_value());
    REQUIRE(graph.channels[0].sends.size() == 1);
    CHECK(graph.channels[0].sends[0].level_db == Catch::Approx(-6.0f));
    CHECK(graph.channels[0].sends[0].pre_fader);
}

TEST_CASE("set_channel_send updates existing send", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)create_aux_bus(graph, AuxBusId{200}, "Reverb");

    (void)set_channel_send(graph, graph.channels[0].id, AuxBusId{200}, -6.0f, false);
    (void)set_channel_send(graph, graph.channels[0].id, AuxBusId{200}, -3.0f, true);

    REQUIRE(graph.channels[0].sends.size() == 1);
    CHECK(graph.channels[0].sends[0].level_db == Catch::Approx(-3.0f));
    CHECK(graph.channels[0].sends[0].pre_fader);
}

TEST_CASE("set_channel_send rejects non-existent aux", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto result = set_channel_send(graph, graph.channels[0].id, AuxBusId{999}, -6.0f);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("set_channel_send bounds the level to the model's gain ceiling",
          "[mix-ir][workflow][regression]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    REQUIRE(create_aux_bus(graph, AuxBusId{200}, "Reverb").has_value());
    const auto channel = graph.channels[0].id;
    CHECK_FALSE(set_channel_send(graph, channel, AuxBusId{200}, 200.0f).has_value());
    CHECK_FALSE(set_channel_send(graph, channel, AuxBusId{200}, std::nanf("")).has_value());
    CHECK(graph.channels[0].sends.empty());
    CHECK(set_channel_send(graph, channel, AuxBusId{200}, MIX_LEVEL_CEILING_DB).has_value());

    // A document carrying an out-of-domain send fails X11.
    graph.channels[0].sends[0].level_db = 200.0f;
    const auto diagnostics = validate_mix(graph);
    CHECK(std::ranges::any_of(diagnostics, [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "X11" && diagnostic.severity == ValidationSeverity::Error;
    }));
}

// =============================================================================
// Effect Chain
// =============================================================================

TEST_CASE("add_channel_effect appends to insert chain", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto result = add_channel_effect(graph, graph.channels[0].id, {MixEffectId{1}, MixEQ{}, true});

    REQUIRE(result.has_value());
    CHECK(graph.channels[0].insert_chain.effects.size() == 1);
}

TEST_CASE("add_channel_effect rejects non-existent channel", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto result = add_channel_effect(graph, ChannelStripId{999}, {MixEffectId{1}, MixEQ{}, true});
    CHECK_FALSE(result.has_value());
}

TEST_CASE("add_bus_effect appends to group insert chain", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)create_group_bus(graph, GroupBusId{100}, "Test", {});
    auto result = add_bus_effect(graph, GroupBusId{100}, {MixEffectId{1}, MixCompressor{}, true});

    REQUIRE(result.has_value());
    CHECK(graph.group_buses[0].insert_chain.effects.size() == 1);
}

TEST_CASE("add_aux_effect appends to aux effect chain", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)create_aux_bus(graph, AuxBusId{200}, "Reverb");
    auto result = add_aux_effect(graph, AuxBusId{200}, {MixEffectId{1}, MixEQ{}, true});

    REQUIRE(result.has_value());
    CHECK(graph.aux_buses[0].effect_chain.effects.size() == 1);
}

TEST_CASE("add_master_effect appends to master chain", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    REQUIRE(add_master_effect(graph, {MixEffectId{1}, MixLimiter{}, true}));
    CHECK(graph.master_bus.insert_chain.effects.size() == 1);
}

TEST_CASE("Mix effect scalar paths are total over materialised parameters",
          "[mix-ir][workflow][device-parameter]") {
    MixEQ eq;
    eq.bands.push_back(
        {1200.0f, -2.0f, 1.5f, MixEQBandType::Peak, DynamicEQConfig{-18.0f, 2.0f, 5.0f, 80.0f}});
    MixEffect effect{MixEffectId{1}, eq, true};

    const auto paths = mix_effect_parameter_paths(effect);
    CHECK(paths.size() == 10);
    for (const auto& path : paths) {
        INFO(path);
        CHECK(get_mix_effect_parameter(effect, path).has_value());
    }
    CHECK_FALSE(get_mix_effect_parameter(effect, "bands[1].frequency").has_value());
    CHECK_FALSE(get_mix_effect_parameter(effect, "bands[0].dynamic.missing").has_value());
}

TEST_CASE("Mix effect path inventory distinguishes non-parameter target residuals",
          "[mix-ir][workflow][device-parameter]") {
    MixCompressor compressor;
    compressor.sidechain.source = SidechainSourceType::ExternalChannel;
    compressor.sidechain.channel_id = ChannelStripId{12};
    MixEffect sidechain{MixEffectId{1}, compressor, true};
    CHECK(mix_effect_non_scalar_paths(sidechain) ==
          std::vector<std::string>{"sidechain.channel_id"});

    MixReverb reverb;
    reverb.algorithm = MixReverbAlgorithm::Convolution;
    reverb.impulse_response = "hall.wav";
    MixEffect convolution{MixEffectId{2}, reverb, true};
    CHECK(mix_effect_non_scalar_paths(convolution) == std::vector<std::string>{"impulse_response"});

    reverb.algorithm = MixReverbAlgorithm::Hall;
    MixEffect hall{MixEffectId{3}, reverb, true};
    CHECK(mix_effect_non_scalar_paths(hall).empty());
    const auto hall_paths = mix_effect_parameter_paths(hall);
    CHECK(std::find(hall_paths.begin(), hall_paths.end(), "shimmer_pitch") == hall_paths.end());

    MixDelay delay;
    delay.tempo_synced = true;
    MixEffect synced{MixEffectId{4}, delay, true};
    const auto synced_paths = mix_effect_parameter_paths(synced);
    CHECK(std::find(synced_paths.begin(), synced_paths.end(), "beat_division") !=
          synced_paths.end());
    CHECK(std::find(synced_paths.begin(), synced_paths.end(), "delay_ms") == synced_paths.end());
}

TEST_CASE("Mix effect mappings validate before graph mutation",
          "[mix-ir][workflow][device-parameter]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    MixCompressor compressor;
    compressor.threshold = -20.0f;
    graph.channels[0].insert_chain.effects.emplace_back(MixEffectId{9}, compressor, true);

    MixDeviceParameter mapping;
    mapping.parameter_name = "Threshold";
    mapping.source_min = -60.0f;
    mapping.source_max = 0.0f;
    mapping.range_min = 0.0f;
    mapping.range_max = 1.0f;
    const auto mapped = map_mix_effect_parameter(graph, MixEffectId{9}, "threshold", mapping);
    REQUIRE(mapped.has_value());
    CHECK(graph.channels[0].insert_chain.effects[0].parameter_map.size() == 1);
    CHECK(map_mix_device_parameter_value(-20.0f, mapping) == Catch::Approx(2.0f / 3.0f));

    mapping.parameter_name = "Ratio";
    CHECK_FALSE(map_mix_effect_parameter(graph, MixEffectId{9}, "missing", mapping).has_value());
    CHECK(graph.channels[0].insert_chain.effects[0].parameter_map.size() == 1);

    mapping.parameter_name = "Threshold";
    CHECK_FALSE(map_mix_effect_parameter(graph, MixEffectId{9}, "ratio", mapping).has_value());
    CHECK(graph.channels[0].insert_chain.effects[0].parameter_map.size() == 1);
}

// =============================================================================
// Level and Spatial
// =============================================================================

TEST_CASE("set_channel_level updates fader", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto result = set_channel_level(graph, graph.channels[0].id, -6.0f);
    REQUIRE(result.has_value());
    CHECK(graph.channels[0].fader.level_db == Catch::Approx(-6.0f));
}

TEST_CASE("set_channel_flags edits only selected authored booleans atomically",
          "[mix-ir][workflow][channel-flags]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}, PartId{2}});
    graph.channels[0].solo = true;
    graph.channels[0].fader.level_db = -6.0f;
    graph.channels[0].input_trim = -7.5f;
    const auto original = mix_to_json(graph);
    REQUIRE(set_channel_flags(graph, graph.channels[0].id, true));
    auto expected = original;
    expected["channels"][0]["mute"] = true;
    CHECK(mix_to_json(graph) == expected);
    REQUIRE(set_channel_flags(graph, graph.channels[0].id, false, false));
    expected["channels"][0]["mute"] = false;
    expected["channels"][0]["solo"] = false;
    CHECK(mix_to_json(graph) == expected);
    REQUIRE(set_channel_flags(graph, graph.channels[1].id, std::nullopt, true));
    expected["channels"][1]["solo"] = true;
    CHECK(mix_to_json(graph) == expected);
    const auto before_failure = mix_to_json(graph);
    auto missing = set_channel_flags(graph, ChannelStripId{900}, true, true);
    REQUIRE_FALSE(missing);
    CHECK(missing.error() == ErrorCode::MixNotFound);
    CHECK(mix_to_json(graph) == before_failure);
    auto empty = set_channel_flags(graph, graph.channels[0].id);
    REQUIRE_FALSE(empty);
    CHECK(empty.error() == ErrorCode::MixInvalidParameter);
    CHECK(mix_to_json(graph) == before_failure);
}

TEST_CASE("set_channel_relative_level sets relative fader", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}, PartId{2}});
    RelativeLevel rel;
    rel.reference.type = LevelReferenceType::Channel;
    rel.reference.channel_id = graph.channels[1].id;
    rel.offset_db = -3.0f;

    auto result = set_channel_relative_level(graph, graph.channels[0].id, rel);
    REQUIRE(result.has_value());
    REQUIRE(graph.channels[0].fader.relative_level.has_value());
    CHECK(graph.channels[0].fader.relative_level->offset_db == Catch::Approx(-3.0f));
}

TEST_CASE("relative fader solver resolves transitive channel group and master constraints",
          "[mix-ir][workflow][relative-level]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}, PartId{2}, PartId{3}});
    graph.channels[1].fader.level_db = -2.0f;

    RelativeLevel first;
    first.reference.type = LevelReferenceType::Channel;
    first.reference.channel_id = graph.channels[1].id;
    first.offset_db = -3.0f;
    REQUIRE(set_channel_relative_level(graph, graph.channels[0].id, first).has_value());

    RelativeLevel transitive;
    transitive.reference.type = LevelReferenceType::Channel;
    transitive.reference.channel_id = graph.channels[0].id;
    transitive.offset_db = -1.0f;
    REQUIRE(set_channel_relative_level(graph, graph.channels[2].id, transitive).has_value());

    GroupBus group;
    group.id = GroupBusId{40};
    group.fader.relative_level = RelativeLevel{
        LevelReference{LevelReferenceType::Channel, -14.0f, graph.channels[2].id, {}, {}}, 2.0f};
    graph.group_buses.push_back(group);
    graph.master_bus.fader.relative_level =
        RelativeLevel{LevelReference{LevelReferenceType::Group, -14.0f, {}, {}, group.id}, -1.0f};

    const auto resolution = resolve_relative_levels(graph);
    REQUIRE(resolution.has_value());
    CHECK(resolution->complete());
    CHECK(resolution->relative_levels_total == 4);
    CHECK(resolution->relative_levels_resolved == 4);
    REQUIRE(resolution->levels.size() == 5);
    CHECK(resolution->levels[0].resolved_level_db == Catch::Approx(-5.0f));
    CHECK(resolution->levels[1].status == FaderLevelResolutionStatus::Explicit);
    CHECK(resolution->levels[2].resolved_level_db == Catch::Approx(-6.0f));
    CHECK(resolution->levels[3].resolved_level_db == Catch::Approx(-4.0f));
    CHECK(resolution->levels[4].resolved_level_db == Catch::Approx(-5.0f));
}

TEST_CASE("loudness-relative faders remain measured residuals and block dependants",
          "[mix-ir][workflow][relative-level]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}, PartId{2}});
    graph.channels[0].fader.level_db = -7.0f;
    graph.channels[0].fader.relative_level =
        RelativeLevel{LevelReference{LevelReferenceType::MasterTarget, -16.0f, {}, {}, {}}, 0.0f};
    graph.channels[1].fader.relative_level = RelativeLevel{
        LevelReference{LevelReferenceType::Channel, -14.0f, graph.channels[0].id, {}, {}}, -2.0f};

    const auto resolution = resolve_relative_levels(graph);
    REQUIRE(resolution.has_value());
    CHECK_FALSE(resolution->complete());
    CHECK(resolution->relative_levels_total == 2);
    CHECK(resolution->relative_levels_resolved == 0);
    CHECK(resolution->relative_levels_unresolved == 2);
    CHECK(resolution->levels[0].status == FaderLevelResolutionStatus::RequiresLoudnessMeasurement);
    CHECK_FALSE(resolution->levels[0].resolved_level_db.has_value());
    CHECK(resolution->levels[1].status == FaderLevelResolutionStatus::BlockedByUnresolvedReference);
}

TEST_CASE("relative fader mutation rejects cycles and missing references transactionally",
          "[mix-ir][workflow][relative-level][transaction]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}, PartId{2}});
    RelativeLevel to_second;
    to_second.reference.type = LevelReferenceType::Channel;
    to_second.reference.channel_id = graph.channels[1].id;
    REQUIRE(set_channel_relative_level(graph, graph.channels[0].id, to_second).has_value());

    RelativeLevel cycle;
    cycle.reference.type = LevelReferenceType::Channel;
    cycle.reference.channel_id = graph.channels[0].id;
    CHECK_FALSE(set_channel_relative_level(graph, graph.channels[1].id, cycle).has_value());
    CHECK_FALSE(graph.channels[1].fader.relative_level.has_value());

    RelativeLevel missing;
    missing.reference.type = LevelReferenceType::Group;
    missing.reference.group_id = GroupBusId{999};
    CHECK_FALSE(set_channel_relative_level(graph, graph.channels[1].id, missing).has_value());
    CHECK_FALSE(graph.channels[1].fader.relative_level.has_value());
}

TEST_CASE("absolute channel level clears relative intent and enforces the fader ceiling",
          "[mix-ir][workflow][relative-level]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}, PartId{2}});
    RelativeLevel relative;
    relative.reference.type = LevelReferenceType::Channel;
    relative.reference.channel_id = graph.channels[1].id;
    REQUIRE(set_channel_relative_level(graph, graph.channels[0].id, relative).has_value());

    REQUIRE(set_channel_level(graph, graph.channels[0].id, -4.0f).has_value());
    CHECK_FALSE(graph.channels[0].fader.relative_level.has_value());
    CHECK(graph.channels[0].fader.level_db == Catch::Approx(-4.0f));
    CHECK_FALSE(set_channel_level(graph, graph.channels[0].id, 12.1f).has_value());
    CHECK(graph.channels[0].fader.level_db == Catch::Approx(-4.0f));
}

TEST_CASE("relative fader solver is stack-bounded for long dependency chains",
          "[mix-ir][workflow][relative-level][adversarial]") {
    constexpr std::size_t channel_count = 4096;
    std::vector<PartId> parts;
    parts.reserve(channel_count);
    for (std::size_t index = 0; index < channel_count; ++index)
        parts.push_back(PartId{index + 1});
    auto graph = create_mix_graph(MixGraphId{1}, parts);
    graph.channels.back().fader.level_db = -1.0f;
    for (std::size_t index = 0; index + 1 < graph.channels.size(); ++index) {
        graph.channels[index].fader.relative_level = RelativeLevel{
            LevelReference{
                LevelReferenceType::Channel, -14.0f, graph.channels[index + 1].id, {}, {}},
            -0.001f};
    }

    const auto resolution = resolve_relative_levels(graph);
    REQUIRE(resolution.has_value());
    CHECK(resolution->complete());
    CHECK(resolution->relative_levels_resolved == channel_count - 1);
    REQUIRE(resolution->levels.front().resolved_level_db.has_value());
    CHECK(*resolution->levels.front().resolved_level_db == Catch::Approx(-5.095f).margin(0.001f));
}

TEST_CASE("set_channel_spatial validates bounds", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});

    SpatialPosition valid;
    valid.pan = -0.5f;
    valid.depth = 0.7f;
    CHECK(set_channel_spatial(graph, graph.channels[0].id, valid).has_value());

    SpatialPosition invalid_pan;
    invalid_pan.pan = 2.0f;
    CHECK_FALSE(set_channel_spatial(graph, graph.channels[0].id, invalid_pan).has_value());

    SpatialPosition invalid_depth;
    invalid_depth.depth = -0.1f;
    CHECK_FALSE(set_channel_spatial(graph, graph.channels[0].id, invalid_depth).has_value());
}

TEST_CASE("set_channel_pan validates range", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    CHECK(set_channel_pan(graph, graph.channels[0].id, 0.5f).has_value());
    CHECK(graph.channels[0].spatial.pan == Catch::Approx(0.5f));

    CHECK_FALSE(set_channel_pan(graph, graph.channels[0].id, 1.5f).has_value());
    CHECK_FALSE(set_channel_pan(graph, graph.channels[0].id, -1.5f).has_value());
}

// =============================================================================
// Intent
// =============================================================================

TEST_CASE("set_channel_intent assigns intent", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    ChannelIntent intent;
    intent.role_in_mix = MixRole::Lead;
    intent.depth_position = DepthPosition::FrontClose;

    auto result = set_channel_intent(graph, graph.channels[0].id, intent);
    REQUIRE(result.has_value());
    REQUIRE(graph.channels[0].intent.has_value());
    CHECK(graph.channels[0].intent->role_in_mix == MixRole::Lead);
}

TEST_CASE("set_group_intent assigns intent", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)create_group_bus(graph, GroupBusId{100}, "Strings", {});

    auto result = set_group_intent(graph, GroupBusId{100}, {"Glue strings", "Violin I prominent"});
    REQUIRE(result.has_value());
    REQUIRE(graph.group_buses[0].intent.has_value());
    CHECK(graph.group_buses[0].intent->function == "Glue strings");
}

// =============================================================================
// Loudness and Output
// =============================================================================

TEST_CASE("set_loudness_target configures master bus", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    REQUIRE(set_loudness_target(graph, {-16.0f, -1.0f, 8.0f, LoudnessStandard::StreamingDynamic})
                .has_value());

    REQUIRE(graph.master_bus.target_loudness.has_value());
    CHECK(graph.master_bus.target_loudness->integrated_lufs == Catch::Approx(-16.0f));
    CHECK(graph.master_bus.target_loudness->standard == LoudnessStandard::StreamingDynamic);
}

TEST_CASE("loudness targets above full scale are rejected without mutation",
          "[mix-ir][workflow][regression]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    CHECK_FALSE(set_loudness_target(graph, {5.0f, -1.0f, std::nullopt, LoudnessStandard::Custom})
                    .has_value());
    CHECK_FALSE(set_loudness_target(graph, {-14.0f, 3.0f, std::nullopt, LoudnessStandard::Custom})
                    .has_value());
    CHECK_FALSE(
        set_loudness_target(graph, {-14.0f, -1.0f, -2.0f, LoudnessStandard::Custom}).has_value());
    CHECK_FALSE(graph.master_bus.target_loudness.has_value());
    // The boundary itself is reachable.
    CHECK(set_loudness_target(graph, {0.0f, 0.0f, 0.0f, LoudnessStandard::Custom}).has_value());

    graph.master_bus.target_loudness->integrated_lufs = 5.0f;
    const auto diagnostics = validate_mix(graph);
    CHECK(std::ranges::any_of(diagnostics, [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "X13" && diagnostic.severity == ValidationSeverity::Error;
    }));
}

TEST_CASE("set_output_format updates both graph and master bus", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    set_output_format(graph, OutputFormat::Surround51);

    CHECK(graph.output_format == OutputFormat::Surround51);
    CHECK(graph.master_bus.output_format == OutputFormat::Surround51);
}

// =============================================================================
// Automation
// =============================================================================

TEST_CASE("add_automation appends automation lane", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    REQUIRE(add_automation(graph,
                           {"channels[1].fader.level_db",
                            {{ScoreTime{1, Beat{0, 1}}, -6.0f}, {ScoreTime{5, Beat{0, 1}}, 0.0f}},
                            InterpolationMode::Linear,
                            "Fade in"})
                .has_value());

    REQUIRE(graph.automation.size() == 1);
    CHECK(graph.automation[0].target == "channels[1].fader.level_db");
    CHECK(graph.automation[0].breakpoints.size() == 2);
}

TEST_CASE("Mix automation targets resolve every structural path family of spec 9.2",
          "[mix-ir][workflow][automation]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{7}});
    REQUIRE(create_aux_bus(graph, AuxBusId{30}, "Reverb").has_value());
    REQUIRE(create_group_bus(graph, GroupBusId{20}, "Strings", {graph.channels[0].id}).has_value());
    REQUIRE(set_channel_send(graph, graph.channels[0].id, AuxBusId{30}, -9.0f).has_value());
    REQUIRE(set_channel_level(graph, graph.channels[0].id, -3.0f).has_value());
    MixCompressor compressor;
    compressor.threshold = -18.0f;
    REQUIRE(add_channel_effect(graph, graph.channels[0].id, {MixEffectId{1}, compressor, true})
                .has_value());
    MixLimiter limiter;
    limiter.ceiling = -1.0f;
    REQUIRE(add_master_effect(graph, {MixEffectId{2}, limiter, true}));

    const auto read = [&](const std::string& path) {
        return get_mix_automation_target(graph, path);
    };
    // Channels are keyed by Score Part ID (7), not by position or ChannelStrip ID.
    CHECK(read("channels[7].fader.level_db") == -3.0f);
    CHECK(read("channels[7].sends[30].level_db") == -9.0f);
    CHECK(read("channels[7].spatial.pan") == 0.0f);
    CHECK(read("channels[7].insert_chain.effects[0].parameters.threshold") == -18.0f);
    CHECK(read("group_buses[20].fader.level_db") == 0.0f);
    CHECK(read("master_bus.fader.level_db") == 0.0f);
    CHECK(read("master_bus.insert_chain.effects[0].parameters.ceiling") == -1.0f);
    CHECK(read("aux_buses[30].return_level") == 0.0f);
    CHECK(read("aux_sends[30].return_level") == 0.0f);

    for (const auto* unresolved : {"channels[0].fader.level_db",
                                   "channels[1].fader.level_db",
                                   "channels[7].fader",
                                   "channels[7].fader.level_db.extra",
                                   "channels[7]..fader.level_db",
                                   "channels[07].fader.level_db",
                                   "channels[7].sends[31].level_db",
                                   "channels[7].insert_chain.effects[1].parameters.threshold",
                                   "channels[7].insert_chain.effects[0].parameters.ceiling",
                                   "master_bus.spatial.pan",
                                   "aux_buses[31].return_level",
                                   ""})
        CHECK_FALSE(read(unresolved).has_value());
}

TEST_CASE("add_automation rejects lanes violating spec 15.3(9) without mutation",
          "[mix-ir][workflow][automation][regression]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    const auto lane = [](std::string target, std::vector<MixAutomationBreakpoint> points) {
        return MixAutomation{std::move(target), std::move(points), InterpolationMode::Linear, {}};
    };
    CHECK_FALSE(add_automation(graph, lane("channels[1].fader.level_db", {})));
    CHECK_FALSE(add_automation(
        graph, lane("channels[1].fader.level_db", {{ScoreTime{0, Beat::zero()}, 0.0f}})));
    CHECK_FALSE(add_automation(
        graph, lane("channels[1].fader.level_db", {{ScoreTime{1, Beat{-1, 4}}, 0.0f}})));
    CHECK_FALSE(add_automation(
        graph,
        lane("channels[1].fader.level_db",
             {{ScoreTime{2, Beat::zero()}, 0.0f}, {ScoreTime{2, Beat::zero()}, 1.0f}})));
    CHECK_FALSE(add_automation(
        graph, lane("channels[1].fader.level_db", {{ScoreTime{1, Beat::zero()}, std::nanf("")}})));
    CHECK_FALSE(add_automation(graph, lane("channels[2].fader.level_db", {{SCORE_START, 0.0f}})));
    CHECK(graph.automation.empty());

    // A lane loaded from a document is held to the same rule.
    graph.automation.push_back(lane("channels[2].fader.level_db", {{SCORE_START, 0.0f}}));
    const auto diagnostics = validate_mix(graph);
    CHECK(std::ranges::any_of(diagnostics, [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "X12" && diagnostic.severity == ValidationSeverity::Error;
    }));
}

// =============================================================================
// Reference Profiles
// =============================================================================

TEST_CASE("add_reference_profile and compare", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});

    ReferenceProfile ref;
    ref.id = ReferenceProfileId{300};
    ref.name = "Reference";
    ref.loudness_profile.integrated = -14.0f;
    ref.dynamic_profile.loudness_range = 6.0f;
    add_reference_profile(graph, ref);

    REQUIRE(set_loudness_target(graph, {-16.0f, -1.0f, 10.0f, LoudnessStandard::StreamingDynamic})
                .has_value());

    auto result = compare_to_reference(graph, ReferenceProfileId{300});
    REQUIRE(result.has_value());
    REQUIRE(result->loudness_difference.has_value());
    CHECK(*result->loudness_difference == Catch::Approx(-2.0f));
    REQUIRE(result->dynamic_range_difference.has_value());
    CHECK(*result->dynamic_range_difference == Catch::Approx(4.0f));
}

TEST_CASE("compare_to_reference never fabricates an unmeasured difference",
          "[mix-ir][workflow][regression]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    ReferenceProfile ref;
    ref.id = ReferenceProfileId{300};
    ref.tonal_balance_curve = {{100.0f, -20.0f}, {1000.0f, -30.0f}};
    ref.spatial_profile.average_width = 0.7f;
    add_reference_profile(graph, ref);

    const auto result = compare_to_reference(graph, ReferenceProfileId{300});
    REQUIRE(result.has_value());
    // The mix has no measured spectrum or width and no configured loudness target.
    CHECK_FALSE(result->spectral_deviation.has_value());
    CHECK_FALSE(result->width_difference.has_value());
    CHECK_FALSE(result->loudness_difference.has_value());
    CHECK_FALSE(result->dynamic_range_difference.has_value());
}

TEST_CASE("compare_to_reference rejects non-existent ref", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto result = compare_to_reference(graph, ReferenceProfileId{999});
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// Seating Templates
// =============================================================================

TEST_CASE("seating positions lie inside every spec 6.4 American interval",
          "[mix-ir][workflow][seating]") {
    struct Interval {
        OrchestralSection section;
        float pan_low, pan_high, depth_low, depth_high;
    };
    // Transcribed from the §6.4 American table.
    const Interval table[] = {
        {OrchestralSection::ViolinI, -0.6f, -0.2f, 0.2f, 0.4f},
        {OrchestralSection::ViolinII, -0.2f, 0.2f, 0.2f, 0.4f},
        {OrchestralSection::Viola, 0.2f, 0.5f, 0.3f, 0.5f},
        {OrchestralSection::Cello, 0.5f, 0.7f, 0.3f, 0.5f},
        {OrchestralSection::DoubleBass, 0.7f, 0.9f, 0.4f, 0.6f},
        {OrchestralSection::Flutes, -0.3f, -0.1f, 0.4f, 0.5f},
        {OrchestralSection::Oboes, -0.1f, 0.1f, 0.4f, 0.5f},
        {OrchestralSection::Clarinets, 0.1f, 0.3f, 0.4f, 0.5f},
        {OrchestralSection::Bassoons, 0.3f, 0.5f, 0.4f, 0.6f},
        {OrchestralSection::Horns, -0.5f, -0.2f, 0.5f, 0.7f},
        {OrchestralSection::Trumpets, -0.2f, 0.2f, 0.6f, 0.7f},
        {OrchestralSection::Trombones, 0.2f, 0.5f, 0.6f, 0.7f},
        {OrchestralSection::Tuba, 0.4f, 0.4f, 0.6f, 0.7f},
        {OrchestralSection::Timpani, 0.3f, 0.5f, 0.7f, 0.8f},
        {OrchestralSection::Percussion, -0.5f, 0.5f, 0.7f, 0.8f},
        {OrchestralSection::Harp, -0.7f, -0.7f, 0.3f, 0.5f},
    };
    for (const auto& row : table) {
        const auto seat = seating_position(SeatingTemplate::American, row.section);
        INFO("section " << static_cast<int>(row.section));
        CHECK(seat.pan >= row.pan_low - 1e-6f);
        CHECK(seat.pan <= row.pan_high + 1e-6f);
        CHECK(seat.depth >= row.depth_low - 1e-6f);
        CHECK(seat.depth <= row.depth_high + 1e-6f);
    }
}

TEST_CASE("European seating moves violin II right and cellos left only",
          "[mix-ir][workflow][seating][regression]") {
    const auto violin_2 = seating_position(SeatingTemplate::European, OrchestralSection::ViolinII);
    CHECK(violin_2.pan >= 0.2f);
    CHECK(violin_2.pan <= 0.6f);
    CHECK(seating_position(SeatingTemplate::European, OrchestralSection::Cello).pan < 0.0f);
    for (const auto section : {OrchestralSection::ViolinI,
                               OrchestralSection::Viola,
                               OrchestralSection::DoubleBass,
                               OrchestralSection::Horns}) {
        CHECK(seating_position(SeatingTemplate::European, section).pan ==
              seating_position(SeatingTemplate::American, section).pan);
    }
}

TEST_CASE("apply_seating_template seats channels by section, not by position",
          "[mix-ir][workflow][seating][regression]") {
    // Channel order deliberately differs from seating order.
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}, PartId{2}, PartId{3}});
    graph.channels[2].spatial.pan = 0.9f; // unassigned: must stay put
    REQUIRE(apply_seating_template(graph,
                                   SeatingTemplate::European,
                                   {{graph.channels[0].id, OrchestralSection::Cello},
                                    {graph.channels[1].id, OrchestralSection::ViolinII}})
                .has_value());
    CHECK(graph.channels[0].spatial.pan < 0.0f);
    CHECK(graph.channels[1].spatial.pan >= 0.2f);
    CHECK(graph.channels[2].spatial.pan == 0.9f);
}

TEST_CASE("apply_seating_template rejects unknown or repeated channels without mutation",
          "[mix-ir][workflow][seating]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    CHECK_FALSE(apply_seating_template(graph,
                                       SeatingTemplate::American,
                                       {{graph.channels[0].id, OrchestralSection::Harp},
                                        {ChannelStripId{99}, OrchestralSection::Viola}})
                    .has_value());
    CHECK_FALSE(apply_seating_template(graph,
                                       SeatingTemplate::American,
                                       {{graph.channels[0].id, OrchestralSection::Harp},
                                        {graph.channels[0].id, OrchestralSection::Viola}})
                    .has_value());
    CHECK(graph.channels[0].spatial.pan == 0.0f);
    CHECK(apply_seating_template(graph, SeatingTemplate::American, {}).has_value());
}

// =============================================================================
// Validation wrapper
// =============================================================================

TEST_CASE("validate returns diagnostics", "[mix-ir][workflow]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto diags = validate(graph);
    // Should at least get X4 (no insert) and I1 (no intent)
    CHECK(diags.size() >= 2);
}

TEST_CASE("Mix effect mutations refuse invalid parameters at every owner atomically",
          "[mix-ir][workflow][authoring]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{17}});
    REQUIRE(create_group_bus(graph, GroupBusId{7}, "Group", {}));
    REQUIRE(create_aux_bus(graph, AuxBusId{8}, "Aux"));
    const auto before = mix_to_json(graph);
    MixCompressor invalid;
    invalid.ratio = 0.25f;
    invalid.attack = -10.0f;
    CHECK_FALSE(add_channel_effect(graph, ChannelStripId{1}, {MixEffectId{5}, invalid, true}));
    CHECK(mix_to_json(graph) == before);
    CHECK_FALSE(add_bus_effect(graph, GroupBusId{7}, {MixEffectId{5}, invalid, true}));
    CHECK(mix_to_json(graph) == before);
    CHECK_FALSE(add_aux_effect(graph, AuxBusId{8}, {MixEffectId{5}, invalid, true}));
    CHECK(mix_to_json(graph) == before);
    CHECK_FALSE(add_master_effect(graph, {MixEffectId{5}, invalid, true}));
    CHECK(mix_to_json(graph) == before);
    MixCompressor boundary;
    boundary.ratio = 1.0f;
    boundary.attack = 0.0f;
    REQUIRE(add_channel_effect(graph, ChannelStripId{1}, {MixEffectId{5}, boundary, true}));
    const auto accepted = mix_to_json(graph);
    CHECK_FALSE(add_master_effect(graph, {MixEffectId{5}, MixLimiter{}, true}));
    CHECK_FALSE(replace_mix_effect(graph, MixEffectId{5}, invalid, true));
    CHECK(mix_to_json(graph) == accepted);
}

TEST_CASE(
    "Mix effect order preserves lane identity and removal requires explicit reference cleanup",
    "[mix-ir][workflow][authoring]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{17}});
    REQUIRE(add_channel_effect(graph, ChannelStripId{1}, {MixEffectId{5}, MixCompressor{}, true}));
    REQUIRE(add_channel_effect(graph, ChannelStripId{1}, {MixEffectId{9}, MixCompressor{}, true}));
    REQUIRE(add_automation(graph,
                           {"channels[17].insert_chain.effects[0].parameters.threshold",
                            {{SCORE_START, -10.0f}},
                            InterpolationMode::Linear,
                            std::nullopt}));
    const auto before = mix_to_json(graph);
    CHECK_FALSE(
        reorder_mix_effects(graph, "channels[17].insert_chain", {MixEffectId{5}, MixEffectId{5}}));
    CHECK(mix_to_json(graph) == before);
    REQUIRE(
        reorder_mix_effects(graph, "channels[17].insert_chain", {MixEffectId{9}, MixEffectId{5}}));
    CHECK(graph.automation[0].target ==
          "channels[17].insert_chain.effects[1].parameters.threshold");
    const auto reordered = mix_to_json(graph);
    CHECK_FALSE(remove_mix_effect(graph, MixEffectId{5}));
    CHECK(mix_to_json(graph) == reordered);
    REQUIRE(remove_mix_effect(graph, MixEffectId{9}));
    CHECK(graph.automation[0].target ==
          "channels[17].insert_chain.effects[0].parameters.threshold");
    REQUIRE(remove_mix_automation(graph, 0));
    REQUIRE(remove_mix_effect(graph, MixEffectId{5}));
    CHECK(graph.channels[0].insert_chain.effects.empty());
}

TEST_CASE("Every admitted Mix effect variant rejects an independent invalid domain fixture",
          "[mix-ir][workflow][authoring]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    const auto before = mix_to_json(graph);
    std::vector<MixEffectParameters> invalid;
    MixEQ eq;
    eq.bands = {MixEQBand{}};
    eq.bands[0].q = 0.0f;
    invalid.emplace_back(eq);
    MixCompressor compressor;
    compressor.ratio = 0.25f;
    invalid.emplace_back(compressor);
    MixGate gate;
    gate.hold = -1.0f;
    invalid.emplace_back(gate);
    MixLimiter limiter;
    limiter.lookahead = -1.0f;
    invalid.emplace_back(limiter);
    MixMultibandDynamics multiband;
    multiband.crossover_frequencies = {1000.0f, 500.0f};
    multiband.bands.resize(3);
    invalid.emplace_back(multiband);
    MixSaturation saturation;
    saturation.mix = 1.01f;
    invalid.emplace_back(saturation);
    MixStereoProcessor stereo;
    stereo.width = -0.1f;
    invalid.emplace_back(stereo);
    MixDelay delay;
    delay.delay_ms = 0.0f;
    invalid.emplace_back(delay);
    MixReverb reverb;
    reverb.pre_delay = -1.0f;
    invalid.emplace_back(reverb);
    limiter = MixLimiter{};
    limiter.algorithm = static_cast<LimiterAlgorithm>(255);
    invalid.emplace_back(limiter);
    for (const auto& parameters : invalid) {
        CAPTURE(parameters.index());
        CHECK_FALSE(add_master_effect(graph, {MixEffectId{1}, parameters, true}));
        CHECK(mix_to_json(graph) == before);
    }
}

TEST_CASE("Mix replacement cannot silently discard automation and retained mapping semantics",
          "[mix-ir][workflow][authoring]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{17}});
    REQUIRE(add_channel_effect(graph, ChannelStripId{1}, {MixEffectId{5}, MixCompressor{}, true}));
    REQUIRE(add_automation(graph,
                           {"channels[17].insert_chain.effects[0].parameters.threshold",
                            {{SCORE_START, -10.0f}},
                            InterpolationMode::Linear,
                            std::nullopt}));
    MixDeviceParameter mapping;
    mapping.parameter_name = "Threshold";
    mapping.source_min = -100.0f;
    mapping.source_max = 0.0f;
    REQUIRE(map_mix_effect_parameter(graph, MixEffectId{5}, "threshold", mapping));
    const auto before = mix_to_json(graph);
    CHECK_FALSE(replace_mix_effect(graph, MixEffectId{5}, MixReverb{}, true));
    CHECK(mix_to_json(graph) == before);
    REQUIRE(remove_mix_automation(graph, 0));
    REQUIRE(remove_mix_parameter_mapping(graph, MixEffectId{5}, "threshold"));
    REQUIRE(replace_mix_effect(graph, MixEffectId{5}, MixReverb{}, true));
}
