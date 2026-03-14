/**
 * @file TSMI005A.cpp
 * @brief Unit tests for Mix IR workflow functions (MIWF001A)
 *
 * Component: TSMI005A
 * Domain: TS (Test) | Category: MI (Mix IR)
 *
 * Tests: MIWF001A
 * Coverage: Graph creation, group bus management, aux bus management,
 *           effect chain mutation, level/spatial control, intent,
 *           loudness targets, automation, reference profiles,
 *           seating templates, validation wrapper
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Mix/MIWF001A.h"

using namespace Sunny::Core;

// =============================================================================
// Graph Construction
// =============================================================================

TEST_CASE("MIWF001A: create_mix_graph produces one channel per part", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1},
        {PartId{10}, PartId{20}, PartId{30}});

    CHECK(graph.id.value == 1);
    REQUIRE(graph.channels.size() == 3);
    CHECK(graph.channels[0].part_id.value == 10);
    CHECK(graph.channels[1].part_id.value == 20);
    CHECK(graph.channels[2].part_id.value == 30);
}

TEST_CASE("MIWF001A: channels have sequential ids", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}, PartId{2}});
    CHECK(graph.channels[0].id.value == 1);
    CHECK(graph.channels[1].id.value == 2);
}

TEST_CASE("MIWF001A: channels default to 0 dB centre pan", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    CHECK(graph.channels[0].fader.level_db == 0.0f);
    CHECK(graph.channels[0].spatial.pan == 0.0f);
}

// =============================================================================
// Group Bus
// =============================================================================

TEST_CASE("MIWF001A: create_group_bus adds group and updates channels", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}, PartId{2}});
    auto result = wf_create_group_bus(graph, GroupBusId{100}, "Strings",
        {graph.channels[0].id, graph.channels[1].id});

    REQUIRE(result.has_value());
    REQUIRE(graph.group_buses.size() == 1);
    CHECK(graph.group_buses[0].name == "Strings");
    CHECK(graph.group_buses[0].member_channels.size() == 2);
    REQUIRE(graph.channels[0].group_assignment.has_value());
    CHECK(graph.channels[0].group_assignment->value == 100);
}

TEST_CASE("MIWF001A: create_group_bus rejects duplicate id", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)wf_create_group_bus(graph, GroupBusId{100}, "A", {graph.channels[0].id});
    auto result = wf_create_group_bus(graph, GroupBusId{100}, "B", {});
    CHECK_FALSE(result.has_value());
}

TEST_CASE("MIWF001A: create_group_bus rejects non-existent members", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto result = wf_create_group_bus(graph, GroupBusId{100}, "X",
        {ChannelStripId{999}});
    CHECK_FALSE(result.has_value());
}

TEST_CASE("MIWF001A: assign_channel_to_group moves channel between groups", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)wf_create_group_bus(graph, GroupBusId{100}, "A", {graph.channels[0].id});
    (void)wf_create_group_bus(graph, GroupBusId{200}, "B", {});

    auto result = wf_assign_channel_to_group(graph, graph.channels[0].id, GroupBusId{200});
    REQUIRE(result.has_value());
    CHECK(graph.channels[0].group_assignment->value == 200);
    CHECK(graph.group_buses[0].member_channels.empty());
    CHECK(graph.group_buses[1].member_channels.size() == 1);
}

// =============================================================================
// Aux Bus
// =============================================================================

TEST_CASE("MIWF001A: create_aux_bus adds aux bus", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto result = wf_create_aux_bus(graph, AuxBusId{200}, "Reverb");
    REQUIRE(result.has_value());
    CHECK(graph.aux_buses.size() == 1);
    CHECK(graph.aux_buses[0].name == "Reverb");
}

TEST_CASE("MIWF001A: create_aux_bus rejects duplicate id", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)wf_create_aux_bus(graph, AuxBusId{200}, "A");
    auto result = wf_create_aux_bus(graph, AuxBusId{200}, "B");
    CHECK_FALSE(result.has_value());
}

TEST_CASE("MIWF001A: set_channel_send configures send", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)wf_create_aux_bus(graph, AuxBusId{200}, "Reverb");

    auto result = wf_set_channel_send(graph, graph.channels[0].id, AuxBusId{200}, -6.0f, true);
    REQUIRE(result.has_value());
    REQUIRE(graph.channels[0].sends.size() == 1);
    CHECK(graph.channels[0].sends[0].level_db == Catch::Approx(-6.0f));
    CHECK(graph.channels[0].sends[0].pre_fader);
}

TEST_CASE("MIWF001A: set_channel_send updates existing send", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)wf_create_aux_bus(graph, AuxBusId{200}, "Reverb");

    (void)wf_set_channel_send(graph, graph.channels[0].id, AuxBusId{200}, -6.0f, false);
    (void)wf_set_channel_send(graph, graph.channels[0].id, AuxBusId{200}, -3.0f, true);

    REQUIRE(graph.channels[0].sends.size() == 1);
    CHECK(graph.channels[0].sends[0].level_db == Catch::Approx(-3.0f));
    CHECK(graph.channels[0].sends[0].pre_fader);
}

TEST_CASE("MIWF001A: set_channel_send rejects non-existent aux", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto result = wf_set_channel_send(graph, graph.channels[0].id, AuxBusId{999}, -6.0f);
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// Effect Chain
// =============================================================================

TEST_CASE("MIWF001A: add_channel_effect appends to insert chain", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto result = wf_add_channel_effect(graph, graph.channels[0].id,
        {MixEffectId{1}, MixEQ{}, true});

    REQUIRE(result.has_value());
    CHECK(graph.channels[0].insert_chain.effects.size() == 1);
}

TEST_CASE("MIWF001A: add_channel_effect rejects non-existent channel", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto result = wf_add_channel_effect(graph, ChannelStripId{999},
        {MixEffectId{1}, MixEQ{}, true});
    CHECK_FALSE(result.has_value());
}

TEST_CASE("MIWF001A: add_bus_effect appends to group insert chain", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)wf_create_group_bus(graph, GroupBusId{100}, "Test", {});
    auto result = wf_add_bus_effect(graph, GroupBusId{100},
        {MixEffectId{1}, MixCompressor{}, true});

    REQUIRE(result.has_value());
    CHECK(graph.group_buses[0].insert_chain.effects.size() == 1);
}

TEST_CASE("MIWF001A: add_aux_effect appends to aux effect chain", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)wf_create_aux_bus(graph, AuxBusId{200}, "Reverb");
    auto result = wf_add_aux_effect(graph, AuxBusId{200},
        {MixEffectId{1}, MixEQ{}, true});

    REQUIRE(result.has_value());
    CHECK(graph.aux_buses[0].effect_chain.effects.size() == 1);
}

TEST_CASE("MIWF001A: add_master_effect appends to master chain", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    wf_add_master_effect(graph, {MixEffectId{1}, MixLimiter{}, true});
    CHECK(graph.master_bus.insert_chain.effects.size() == 1);
}

// =============================================================================
// Level and Spatial
// =============================================================================

TEST_CASE("MIWF001A: set_channel_level updates fader", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto result = wf_set_channel_level(graph, graph.channels[0].id, -6.0f);
    REQUIRE(result.has_value());
    CHECK(graph.channels[0].fader.level_db == Catch::Approx(-6.0f));
}

TEST_CASE("MIWF001A: set_channel_relative_level sets relative fader", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}, PartId{2}});
    RelativeLevel rel;
    rel.reference.type = LevelReferenceType::Channel;
    rel.reference.channel_id = graph.channels[1].id;
    rel.offset_db = -3.0f;

    auto result = wf_set_channel_relative_level(graph, graph.channels[0].id, rel);
    REQUIRE(result.has_value());
    REQUIRE(graph.channels[0].fader.relative_level.has_value());
    CHECK(graph.channels[0].fader.relative_level->offset_db == Catch::Approx(-3.0f));
}

TEST_CASE("MIWF001A: set_channel_spatial validates bounds", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});

    SpatialPosition valid;
    valid.pan = -0.5f;
    valid.depth = 0.7f;
    CHECK(wf_set_channel_spatial(graph, graph.channels[0].id, valid).has_value());

    SpatialPosition invalid_pan;
    invalid_pan.pan = 2.0f;
    CHECK_FALSE(wf_set_channel_spatial(graph, graph.channels[0].id, invalid_pan).has_value());

    SpatialPosition invalid_depth;
    invalid_depth.depth = -0.1f;
    CHECK_FALSE(wf_set_channel_spatial(graph, graph.channels[0].id, invalid_depth).has_value());
}

TEST_CASE("MIWF001A: set_channel_pan validates range", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    CHECK(wf_set_channel_pan(graph, graph.channels[0].id, 0.5f).has_value());
    CHECK(graph.channels[0].spatial.pan == Catch::Approx(0.5f));

    CHECK_FALSE(wf_set_channel_pan(graph, graph.channels[0].id, 1.5f).has_value());
    CHECK_FALSE(wf_set_channel_pan(graph, graph.channels[0].id, -1.5f).has_value());
}

// =============================================================================
// Intent
// =============================================================================

TEST_CASE("MIWF001A: set_channel_intent assigns intent", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    ChannelIntent intent;
    intent.role_in_mix = MixRole::Lead;
    intent.depth_position = DepthPosition::FrontClose;

    auto result = wf_set_channel_intent(graph, graph.channels[0].id, intent);
    REQUIRE(result.has_value());
    REQUIRE(graph.channels[0].intent.has_value());
    CHECK(graph.channels[0].intent->role_in_mix == MixRole::Lead);
}

TEST_CASE("MIWF001A: set_group_intent assigns intent", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    (void)wf_create_group_bus(graph, GroupBusId{100}, "Strings", {});

    auto result = wf_set_group_intent(graph, GroupBusId{100},
        {"Glue strings", "Violin I prominent"});
    REQUIRE(result.has_value());
    REQUIRE(graph.group_buses[0].intent.has_value());
    CHECK(graph.group_buses[0].intent->function == "Glue strings");
}

// =============================================================================
// Loudness and Output
// =============================================================================

TEST_CASE("MIWF001A: set_loudness_target configures master bus", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    wf_set_loudness_target(graph, {-16.0f, -1.0f, 8.0f, LoudnessStandard::StreamingDynamic});

    REQUIRE(graph.master_bus.target_loudness.has_value());
    CHECK(graph.master_bus.target_loudness->integrated_lufs == Catch::Approx(-16.0f));
    CHECK(graph.master_bus.target_loudness->standard == LoudnessStandard::StreamingDynamic);
}

TEST_CASE("MIWF001A: set_output_format updates both graph and master bus", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    wf_set_output_format(graph, OutputFormat::Surround51);

    CHECK(graph.output_format == OutputFormat::Surround51);
    CHECK(graph.master_bus.output_format == OutputFormat::Surround51);
}

// =============================================================================
// Automation
// =============================================================================

TEST_CASE("MIWF001A: add_automation appends automation lane", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    wf_add_automation(graph, {
        "channels[0].fader.level_db",
        {{ScoreTime{1, Beat{0, 1}}, -6.0f}, {ScoreTime{5, Beat{0, 1}}, 0.0f}},
        InterpolationMode::Linear,
        "Fade in"
    });

    REQUIRE(graph.automation.size() == 1);
    CHECK(graph.automation[0].target == "channels[0].fader.level_db");
    CHECK(graph.automation[0].breakpoints.size() == 2);
}

// =============================================================================
// Reference Profiles
// =============================================================================

TEST_CASE("MIWF001A: add_reference_profile and compare", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});

    ReferenceProfile ref;
    ref.id = ReferenceProfileId{300};
    ref.name = "Reference";
    ref.loudness_profile.integrated = -14.0f;
    ref.dynamic_profile.loudness_range = 6.0f;
    wf_add_reference_profile(graph, ref);

    wf_set_loudness_target(graph, {-16.0f, -1.0f, 10.0f, LoudnessStandard::StreamingDynamic});

    auto result = wf_compare_to_reference(graph, ReferenceProfileId{300});
    REQUIRE(result.has_value());
    CHECK(result->loudness_difference == Catch::Approx(-2.0f));
    CHECK(result->dynamic_range_difference == Catch::Approx(4.0f));
}

TEST_CASE("MIWF001A: compare_to_reference rejects non-existent ref", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto result = wf_compare_to_reference(graph, ReferenceProfileId{999});
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// Seating Templates
// =============================================================================

TEST_CASE("MIWF001A: apply_seating_template American distributes positions", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1},
        {PartId{1}, PartId{2}, PartId{3}, PartId{4}});
    wf_apply_seating_template(graph, SeatingTemplate::American);

    // First channel should be left, last should be right
    CHECK(graph.channels[0].spatial.pan < 0.0f);
    CHECK(graph.channels[3].spatial.pan > 0.0f);

    // Depth should increase left to right
    CHECK(graph.channels[0].spatial.depth < graph.channels[3].spatial.depth);
}

TEST_CASE("MIWF001A: apply_seating_template European wider spread", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1},
        {PartId{1}, PartId{2}});
    wf_apply_seating_template(graph, SeatingTemplate::European);

    // European seating should have wider pan range
    CHECK(graph.channels[0].spatial.pan == Catch::Approx(-0.9f));
    CHECK(graph.channels[1].spatial.pan == Catch::Approx(0.9f));
}

TEST_CASE("MIWF001A: apply_seating_template handles single channel", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    wf_apply_seating_template(graph, SeatingTemplate::American);
    // Should centre the single channel
    CHECK(graph.channels[0].spatial.pan == Catch::Approx(0.0f));
}

TEST_CASE("MIWF001A: apply_seating_template handles empty graph", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {});
    wf_apply_seating_template(graph, SeatingTemplate::American);
    CHECK(graph.channels.empty());
}

// =============================================================================
// Validation wrapper
// =============================================================================

TEST_CASE("MIWF001A: wf_validate returns diagnostics", "[mix-ir][workflow]") {
    auto graph = wf_create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto diags = wf_validate(graph);
    // Should at least get X4 (no insert) and I1 (no intent)
    CHECK(diags.size() >= 2);
}
