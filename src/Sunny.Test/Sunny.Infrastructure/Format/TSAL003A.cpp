/**
 * @file TSAL003A.cpp
 * @brief Unit tests for Mix IR → Ableton compiler (FMAL003A)
 *
 * Component: TSAL003A
 * Domain: TS (Test) | Category: AL (Ableton Live)
 *
 * Tests: FMAL003A
 * Coverage: Group/return track creation, effect insertion,
 *           channel configuration, routing, master bus, automation
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Format/FMAL003A.h"
#include "Bridge/INTP001A.h"

using namespace Sunny::Infrastructure;
using namespace Sunny::Infrastructure::Format;
using namespace Sunny::Core;

namespace {

/// Create a minimal MixGraph with two channels
MixGraph make_test_graph() {
    MixGraph g;
    g.id = MixGraphId{1};

    ChannelStrip ch1;
    ch1.id = ChannelStripId{1};
    ch1.part_id = PartId{1};
    ch1.fader.level_db = -3.0f;
    ch1.spatial.pan = -0.5f;
    g.channels.push_back(ch1);

    ChannelStrip ch2;
    ch2.id = ChannelStripId{2};
    ch2.part_id = PartId{2};
    ch2.fader.level_db = -6.0f;
    ch2.spatial.pan = 0.5f;
    g.channels.push_back(ch2);

    return g;
}

/// Create a MixGraph with group bus, aux bus, and effects
MixGraph make_full_graph() {
    auto g = make_test_graph();

    // Add a group bus
    GroupBus grp;
    grp.id = GroupBusId{1};
    grp.name = "Strings";
    grp.member_channels = {ChannelStripId{1}, ChannelStripId{2}};
    grp.fader.level_db = 0.0f;

    // Add EQ to group
    MixEffect eq;
    eq.id = MixEffectId{1};
    eq.parameters = MixEQ{};
    grp.insert_chain.effects.push_back(eq);
    g.group_buses.push_back(grp);

    // Add an aux bus (reverb return)
    AuxBus aux;
    aux.id = AuxBusId{1};
    aux.name = "Hall Reverb";
    aux.return_level = -10.0f;
    g.aux_buses.push_back(aux);

    // Add channel sends
    AuxSendLevel send;
    send.aux_bus_id = AuxBusId{1};
    send.level_db = -12.0f;
    send.enabled = true;
    g.channels[0].sends.push_back(send);
    g.channels[1].sends.push_back(send);

    // Add EQ to channel 1
    MixEffect ch_eq;
    ch_eq.id = MixEffectId{2};
    ch_eq.parameters = MixEQ{};
    g.channels[0].insert_chain.effects.push_back(ch_eq);

    // Add compressor to master
    MixEffect master_comp;
    master_comp.id = MixEffectId{3};
    master_comp.parameters = MixCompressor{};
    g.master_bus.insert_chain.effects.push_back(master_comp);

    return g;
}

}  // namespace

// =============================================================================
// Basic compilation
// =============================================================================

TEST_CASE("FMAL003A: minimal graph configures channels", "[ableton][mix]") {
    auto graph = make_test_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->channels_configured == 2);
}

TEST_CASE("FMAL003A: channel volume is set from fader", "[ableton][mix]") {
    auto graph = make_test_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    int volume_count = 0;
    for (const auto* e : sets) {
        if (e->request.property_or_method == "volume")
            volume_count++;
    }
    // 2 channel volumes + master volume
    CHECK(volume_count >= 2);
}

TEST_CASE("FMAL003A: channel pan is set from spatial", "[ableton][mix]") {
    auto graph = make_test_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    int pan_count = 0;
    for (const auto* e : sets) {
        if (e->request.property_or_method == "panning")
            pan_count++;
    }
    CHECK(pan_count == 2);
}

// =============================================================================
// Group and return tracks
// =============================================================================

TEST_CASE("FMAL003A: group tracks created for group buses", "[ableton][mix]") {
    auto graph = make_full_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->group_tracks_created == 1);

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    bool found = false;
    for (const auto* e : calls) {
        if (e->request.property_or_method == "create_group_track")
            found = true;
    }
    CHECK(found);
}

TEST_CASE("FMAL003A: return tracks created for aux buses", "[ableton][mix]") {
    auto graph = make_full_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->return_tracks_created == 1);

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    bool found = false;
    for (const auto* e : calls) {
        if (e->request.property_or_method == "create_return_track")
            found = true;
    }
    CHECK(found);
}

// =============================================================================
// Effect insertion
// =============================================================================

TEST_CASE("FMAL003A: effects inserted on channels, groups, master", "[ableton][mix]") {
    auto graph = make_full_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());
    // 1 channel EQ + 1 group EQ + 1 master compressor = 3
    CHECK(r->effects_inserted == 3);
}

TEST_CASE("FMAL003A: EQ maps to EQ Eight device", "[ableton][mix]") {
    auto graph = make_full_graph();
    CommandBuffer buf;

    compile_mix_to_ableton(graph, 0, buf);

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    bool found_eq = false;
    for (const auto* e : calls) {
        if (e->request.property_or_method == "load_device") {
            auto* arg = std::get_if<std::string>(&e->request.args[0]);
            if (arg && *arg == "EQ Eight") found_eq = true;
        }
    }
    CHECK(found_eq);
}

TEST_CASE("FMAL003A: Compressor maps to Compressor device", "[ableton][mix]") {
    auto graph = make_full_graph();
    CommandBuffer buf;

    compile_mix_to_ableton(graph, 0, buf);

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    bool found_comp = false;
    for (const auto* e : calls) {
        if (e->request.property_or_method == "load_device") {
            auto* arg = std::get_if<std::string>(&e->request.args[0]);
            if (arg && *arg == "Compressor") found_comp = true;
        }
    }
    CHECK(found_comp);
}

// =============================================================================
// Sends
// =============================================================================

TEST_CASE("FMAL003A: send levels configured", "[ableton][mix]") {
    auto graph = make_full_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->sends_configured == 2);
}

// =============================================================================
// Master bus
// =============================================================================

TEST_CASE("FMAL003A: master fader set", "[ableton][mix]") {
    auto graph = make_test_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    bool found_master_vol = false;
    for (const auto* e : sets) {
        if (e->request.path.to_string().find("master_track") != std::string::npos &&
            e->request.property_or_method == "volume") {
            found_master_vol = true;
        }
    }
    CHECK(found_master_vol);
}

// =============================================================================
// Automation
// =============================================================================

TEST_CASE("FMAL003A: automation lanes created", "[ableton][mix]") {
    auto graph = make_test_graph();
    MixAutomation ma;
    ma.target = "channels[0].fader.level_db";
    graph.automation.push_back(ma);

    CommandBuffer buf;
    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->automation_lanes == 1);
}

// =============================================================================
// Mute/Solo
// =============================================================================

TEST_CASE("FMAL003A: muted channel sends mute property", "[ableton][mix]") {
    auto graph = make_test_graph();
    graph.channels[0].mute = true;
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    bool found_mute = false;
    for (const auto* e : sets) {
        if (e->request.property_or_method == "mute") found_mute = true;
    }
    CHECK(found_mute);
}
