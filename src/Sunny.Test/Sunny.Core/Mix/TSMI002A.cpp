/**
 * @file TSMI002A.cpp
 * @brief Unit tests for Mix IR document model (MIDC001A)
 *
 * Component: TSMI002A
 * Domain: TS (Test) | Category: MI (Mix IR)
 *
 * Tests: MIDC001A
 * Coverage: ChannelStrip, GroupBus, AuxBus, MasterBus, ReferenceProfile,
 *           MixGraph construction and structural properties
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Mix/MIDC001A.h"

using namespace Sunny::Core;

// =============================================================================
// ChannelStrip
// =============================================================================

TEST_CASE("MIDC001A: ChannelStrip default values", "[mix-ir][document]") {
    ChannelStrip ch;
    CHECK(ch.input_trim == 0.0f);
    CHECK_FALSE(ch.polarity_invert);
    CHECK(ch.insert_chain.effects.empty());
    CHECK(ch.fader.level_db == 0.0f);
    CHECK(ch.spatial.pan == 0.0f);
    CHECK_FALSE(ch.mute);
    CHECK_FALSE(ch.solo);
    CHECK(ch.sends.empty());
    CHECK_FALSE(ch.group_assignment.has_value());
    CHECK_FALSE(ch.intent.has_value());
}

TEST_CASE("MIDC001A: ChannelStrip with group assignment", "[mix-ir][document]") {
    ChannelStrip ch;
    ch.id = ChannelStripId{1};
    ch.part_id = PartId{10};
    ch.group_assignment = GroupBusId{100};

    CHECK(ch.id.value == 1);
    CHECK(ch.part_id.value == 10);
    REQUIRE(ch.group_assignment.has_value());
    CHECK(ch.group_assignment->value == 100);
}

TEST_CASE("MIDC001A: ChannelStrip with insert chain", "[mix-ir][document]") {
    ChannelStrip ch;
    ch.insert_chain.effects.push_back({
        MixEffectId{1},
        MixEQ{{{80.0f, 0.0f, 0.7f, MixEQBandType::LowCut, std::nullopt}}, false, false},
        true
    });
    ch.insert_chain.effects.push_back({
        MixEffectId{2},
        MixCompressor{},
        true
    });

    CHECK(ch.insert_chain.effects.size() == 2);
    CHECK(std::holds_alternative<MixEQ>(ch.insert_chain.effects[0].parameters));
    CHECK(std::holds_alternative<MixCompressor>(ch.insert_chain.effects[1].parameters));
}

// =============================================================================
// GroupBus
// =============================================================================

TEST_CASE("MIDC001A: GroupBus default values", "[mix-ir][document]") {
    GroupBus gb;
    CHECK(gb.name.empty());
    CHECK(gb.member_channels.empty());
    CHECK(gb.member_groups.empty());
    CHECK(gb.output.type == GroupOutputType::Master);
    CHECK_FALSE(gb.intent.has_value());
}

TEST_CASE("MIDC001A: GroupBus with members and intent", "[mix-ir][document]") {
    GroupBus gb;
    gb.id = GroupBusId{50};
    gb.name = "Strings";
    gb.member_channels = {ChannelStripId{1}, ChannelStripId{2}, ChannelStripId{3}};
    gb.intent = GroupIntent{
        "Glue the string section together",
        "Violin I slightly prominent, violas and cellos supporting"
    };

    CHECK(gb.member_channels.size() == 3);
    REQUIRE(gb.intent.has_value());
    CHECK(gb.intent->function == "Glue the string section together");
}

TEST_CASE("MIDC001A: GroupBus nested routing", "[mix-ir][document]") {
    GroupBus parent;
    parent.id = GroupBusId{100};
    parent.name = "All Strings";
    parent.output.type = GroupOutputType::Master;

    GroupBus child;
    child.id = GroupBusId{101};
    child.name = "High Strings";
    child.output.type = GroupOutputType::Group;
    child.output.parent_group_id = parent.id;

    parent.member_groups.push_back(child.id);

    CHECK(child.output.type == GroupOutputType::Group);
    CHECK(child.output.parent_group_id == parent.id);
    CHECK(parent.member_groups.size() == 1);
}

// =============================================================================
// AuxBus
// =============================================================================

TEST_CASE("MIDC001A: AuxBus default values", "[mix-ir][document]") {
    AuxBus ab;
    CHECK(ab.return_level == 0.0f);
    CHECK(ab.output.type == AuxOutputType::Master);
    CHECK_FALSE(ab.intent.has_value());
}

TEST_CASE("MIDC001A: AuxBus reverb configuration", "[mix-ir][document]") {
    AuxBus ab;
    ab.id = AuxBusId{200};
    ab.name = "Concert Hall";
    ab.intent = "Shared concert hall space for all orchestral instruments";

    // Add a reverb effect (modelled as a compressor placeholder —
    // in practice this would be a dedicated reverb type; here we verify
    // the chain structure)
    ab.effect_chain.effects.push_back({
        MixEffectId{10},
        MixEQ{},
        true
    });

    CHECK(ab.name == "Concert Hall");
    CHECK(ab.effect_chain.effects.size() == 1);
    REQUIRE(ab.intent.has_value());
}

// =============================================================================
// MasterBus
// =============================================================================

TEST_CASE("MIDC001A: MasterBus default values", "[mix-ir][document]") {
    MasterBus mb;
    CHECK(mb.output_format == OutputFormat::Stereo);
    CHECK_FALSE(mb.dithering.has_value());
    CHECK_FALSE(mb.target_loudness.has_value());
    CHECK(mb.metering.lufs);
}

TEST_CASE("MIDC001A: MasterBus with loudness target and dithering", "[mix-ir][document]") {
    MasterBus mb;
    mb.target_loudness = LoudnessTarget{-16.0f, -1.0f, 8.0f, LoudnessStandard::StreamingDynamic};
    mb.dithering = DitheringConfig{16, DitherAlgorithm::TPDF, 3, 2, true};

    REQUIRE(mb.target_loudness.has_value());
    CHECK(mb.target_loudness->integrated_lufs == -16.0f);
    CHECK(mb.target_loudness->standard == LoudnessStandard::StreamingDynamic);
    REQUIRE(mb.dithering.has_value());
    CHECK(mb.dithering->target_bit_depth == 16);
}

// =============================================================================
// ReferenceProfile
// =============================================================================

TEST_CASE("MIDC001A: ReferenceProfile construction", "[mix-ir][document]") {
    ReferenceProfile rp;
    rp.id = ReferenceProfileId{300};
    rp.name = "Mahler Symphony No. 2 — Bernstein/VPO";
    rp.source = "/references/mahler2_bernstein.wav";
    rp.loudness_profile.integrated = -18.0f;
    rp.loudness_profile.true_peak = -3.0f;
    rp.spatial_profile.average_width = 0.7f;
    rp.tonal_balance_curve = {{100.0f, -2.0f}, {1000.0f, 0.0f}, {10000.0f, -4.0f}};

    CHECK(rp.name == "Mahler Symphony No. 2 — Bernstein/VPO");
    CHECK(rp.loudness_profile.integrated == -18.0f);
    CHECK(rp.tonal_balance_curve.size() == 3);
}

// =============================================================================
// MixGraph
// =============================================================================

TEST_CASE("MIDC001A: MixGraph default values", "[mix-ir][document]") {
    MixGraph graph;
    CHECK(graph.channels.empty());
    CHECK(graph.group_buses.empty());
    CHECK(graph.aux_buses.empty());
    CHECK(graph.output_format == OutputFormat::Stereo);
    CHECK(graph.max_group_nesting_depth == 4);
}

TEST_CASE("MIDC001A: MixGraph with full structure", "[mix-ir][document]") {
    MixGraph graph;
    graph.id = MixGraphId{1};

    // Two channels
    ChannelStrip ch1;
    ch1.id = ChannelStripId{1};
    ch1.part_id = PartId{10};
    ch1.fader.level_db = -3.0f;
    ch1.spatial.pan = -0.5f;

    ChannelStrip ch2;
    ch2.id = ChannelStripId{2};
    ch2.part_id = PartId{11};
    ch2.fader.level_db = 0.0f;
    ch2.spatial.pan = 0.5f;

    graph.channels = {ch1, ch2};

    // One group bus
    GroupBus strings;
    strings.id = GroupBusId{100};
    strings.name = "Strings";
    strings.member_channels = {ch1.id, ch2.id};
    graph.group_buses.push_back(strings);

    // Assign channels
    graph.channels[0].group_assignment = strings.id;
    graph.channels[1].group_assignment = strings.id;

    // One aux bus
    AuxBus reverb;
    reverb.id = AuxBusId{200};
    reverb.name = "Hall Reverb";
    graph.aux_buses.push_back(reverb);

    // Add sends
    graph.channels[0].sends.push_back({reverb.id, -6.0f, false, true});
    graph.channels[1].sends.push_back({reverb.id, -3.0f, false, true});

    CHECK(graph.channels.size() == 2);
    CHECK(graph.group_buses.size() == 1);
    CHECK(graph.aux_buses.size() == 1);
    CHECK(graph.channels[0].sends.size() == 1);
    CHECK(graph.channels[0].sends[0].level_db == -6.0f);
}

TEST_CASE("MIDC001A: MixGraph annotations and automation", "[mix-ir][document]") {
    MixGraph graph;
    graph.mix_annotations.push_back({"master", "Target: warm, full mix with wide stereo image"});
    graph.automation.push_back({
        "channels[0].fader.level_db",
        {{ScoreTime{1, Beat{0, 1}}, -6.0f}, {ScoreTime{5, Beat{0, 1}}, 0.0f}},
        InterpolationMode::Linear,
        "Gradual fade in"
    });

    CHECK(graph.mix_annotations.size() == 1);
    CHECK(graph.automation.size() == 1);
    CHECK(graph.automation[0].breakpoints.size() == 2);
}
