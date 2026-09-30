/**
 * @file ir_tools_test.cpp
 * @brief MCP-level regression tests for the Timbre and Mix IR tools
 *
 * Each case drives the public tool surface and states the contract it
 * verifies: the Mix automation, seating, reference-comparison and level
 * domains (issue #13), the Timbre effect-identity, preset atomicity and
 * compiler-gap contracts (issue #14), and plan-time refusal of values Live
 * cannot represent (issue #7).
 */

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sunny/infrastructure/mcp/mix_tools.hpp>
#include <sunny/infrastructure/mcp/server.hpp>
#include <sunny/infrastructure/mcp/session.hpp>
#include <sunny/infrastructure/mcp/timbre_tools.hpp>
#include <variant>

using namespace sunny::infrastructure;
using json = nlohmann::json;

namespace {

json call_tool(McpServer& server, const std::string& name, const json& arguments, int id) {
    auto response = server.process_request({{"jsonrpc", "2.0"},
                                            {"method", "tools/call"},
                                            {"params", {{"name", name}, {"arguments", arguments}}},
                                            {"id", id}});
    INFO("tool response for " << name << ": " << response.dump());
    REQUIRE(response.contains("result"));
    return json::parse(response["result"]["content"][0]["text"].get<std::string>());
}

bool succeeded(const json& response) {
    return response.contains("success") && response["success"] == true &&
           !response.contains("error");
}

bool any_warning_contains(const json& response, const std::string& needle) {
    if (!response.contains("warnings")) return false;
    return std::ranges::any_of(response["warnings"], [&](const json& warning) {
        return warning.get<std::string>().find(needle) != std::string::npos;
    });
}

bool any_command_mentions(const CommandBuffer& buffer, const std::string& needle) {
    return std::ranges::any_of(buffer.entries(), [&](const CommandBuffer::Entry& entry) {
        if (entry.request.property_or_method.find(needle) != std::string::npos) return true;
        return std::ranges::any_of(entry.request.args, [&](const auto& argument) {
            const auto* text = std::get_if<std::string>(&argument);
            return text != nullptr && text->find(needle) != std::string::npos;
        });
    });
}

// =============================================================================
// Mix (#13)
// =============================================================================

TEST_CASE("add_mix_automation rejects a zero beat denominator and the server keeps answering",
          "[mcp][mix][automation][regression]") {
    McpServer server;
    register_mix_tools(server);
    const auto graph = call_tool(server, "create_mix_graph", {{"part_ids", {1}}}, 1);

    const auto rejected = call_tool(
        server,
        "add_mix_automation",
        {{"graph_id", graph["graph_id"]},
         {"target", "channels[1].fader.level_db"},
         {"breakpoints", {{{"bar", 1}, {"beat_num", 1}, {"beat_den", 0}, {"value", -6.0}}}}},
        2);
    CHECK(rejected.contains("error"));

    const auto validation = call_tool(server, "validate_mix", {{"graph_id", graph["graph_id"]}}, 3);
    CHECK(validation["valid"] == true);
    const auto document = call_tool(server, "get_mix_json", {{"graph_id", graph["graph_id"]}}, 4);
    CHECK(document["mix_ir"]["automation"].empty());
}

TEST_CASE("add_mix_automation admits only lanes satisfying spec 15.3(9)",
          "[mcp][mix][automation][regression]") {
    McpServer server;
    register_mix_tools(server);
    const auto graph = call_tool(server, "create_mix_graph", {{"part_ids", {1}}}, 10);
    const auto add = [&](const std::string& target, const json& breakpoints, int id) {
        return call_tool(
            server,
            "add_mix_automation",
            {{"graph_id", graph["graph_id"]}, {"target", target}, {"breakpoints", breakpoints}},
            id);
    };

    // Bar 0 precedes SCORE_START (bar 1, beat 0).
    CHECK(add("channels[1].fader.level_db", {{{"bar", 0}, {"value", -6.0}}}, 11).contains("error"));
    // A negative beat offset also precedes the bar it names, hence the score start.
    CHECK(add("channels[1].fader.level_db",
              {{{"bar", 1}, {"beat_num", -1}, {"beat_den", 4}, {"value", -6.0}}},
              12)
              .contains("error"));
    // Times must strictly increase.
    CHECK(add("channels[1].fader.level_db",
              {{{"bar", 3}, {"value", -6.0}}, {{"bar", 2}, {"value", 0.0}}},
              13)
              .contains("error"));
    // Part 9 has no channel, and "loudness" is not a fader field.
    CHECK(add("channels[9].fader.level_db", {{{"bar", 1}, {"value", -6.0}}}, 14).contains("error"));
    CHECK(add("channels[1].fader.loudness", {{{"bar", 1}, {"value", -6.0}}}, 15).contains("error"));
    CHECK(add("channels[1].fader.level_db", json::array(), 16).contains("error"));

    const auto document = call_tool(server, "get_mix_json", {{"graph_id", graph["graph_id"]}}, 17);
    CHECK(document["mix_ir"]["automation"].empty());

    // A well-formed lane on Part 1's fader is admitted.
    CHECK(succeeded(add("channels[1].fader.level_db",
                        {{{"bar", 1}, {"value", -12.0}},
                         {{"bar", 1}, {"beat_num", 1}, {"beat_den", 2}, {"value", -9.0}},
                         {{"bar", 2}, {"value", -6.0}}},
                        18)));
    CHECK(call_tool(server, "validate_mix", {{"graph_id", graph["graph_id"]}}, 19)["valid"] ==
          true);
}

json seat(McpServer& server, int seating_template, int id) {
    const auto graph = call_tool(server, "create_mix_graph", {{"part_ids", {1, 2, 3, 4}}}, id);
    const auto applied = call_tool(server,
                                   "apply_seating_template",
                                   {{"graph_id", graph["graph_id"]},
                                    {"template", seating_template},
                                    {"sections",
                                     {{{"channel_id", 1}, {"section", "violin_1"}},
                                      {{"channel_id", 2}, {"section", "violin_2"}},
                                      {{"channel_id", 3}, {"section", "viola"}},
                                      {{"channel_id", 4}, {"section", "cello"}}}}},
                                   id + 1);
    REQUIRE(succeeded(applied));
    return call_tool(server, "get_mix_json", {{"graph_id", graph["graph_id"]}}, id + 2)["mix_ir"];
}

float pan_of(const json& mix, std::size_t channel_index) {
    return mix["channels"][channel_index]["spatial"]["pan"].get<float>();
}

TEST_CASE("European seating places violin II right and cellos left per spec 6.4",
          "[mcp][mix][seating][regression]") {
    McpServer server;
    register_mix_tools(server);
    const auto mix = seat(server, 1, 20);
    const float violin_2 = pan_of(mix, 1);
    const float cello = pan_of(mix, 3);
    CHECK(violin_2 >= 0.2f);
    CHECK(violin_2 <= 0.6f);
    CHECK(cello < 0.0f);
}

TEST_CASE("American seating places each string section inside its spec 6.4 pan interval",
          "[mcp][mix][seating][regression]") {
    McpServer server;
    register_mix_tools(server);
    const auto mix = seat(server, 0, 30);
    const auto within = [](float value, float low, float high) {
        return value >= low && value <= high;
    };
    CHECK(within(pan_of(mix, 0), -0.6f, -0.2f));
    CHECK(within(pan_of(mix, 1), -0.2f, 0.2f));
    CHECK(within(pan_of(mix, 2), 0.2f, 0.5f));
    CHECK(within(pan_of(mix, 3), 0.5f, 0.7f));
}

TEST_CASE("compare_to_reference reports unmeasured differences as unavailable",
          "[mcp][mix][reference][regression]") {
    McpServer server;
    register_mix_tools(server);
    const auto graph = call_tool(server, "create_mix_graph", {{"part_ids", {1}}}, 40);
    const auto reference = call_tool(
        server,
        "create_reference_profile",
        {{"graph_id", graph["graph_id"]},
         {"name", "Reference"},
         {"integrated_lufs", -14.0},
         {"avg_width", 0.7},
         {"tonal_balance",
          {{{"frequency", 100.0}, {"level", -20.0}}, {{"frequency", 1000.0}, {"level", -30.0}}}}},
        41);
    const auto comparison =
        call_tool(server,
                  "compare_to_reference",
                  {{"graph_id", graph["graph_id"]}, {"reference_id", reference["reference_id"]}},
                  42);
    INFO(comparison.dump());
    // No mix spectrum or width has been measured, so neither difference exists.
    CHECK(comparison["spectral_deviation"].is_null());
    CHECK(comparison["width_difference"].is_null());
    // No loudness target is configured either.
    CHECK(comparison["loudness_difference"].is_null());
}

TEST_CASE("loudness targets and sends are bounded to their physical domains",
          "[mcp][mix][loudness][regression]") {
    McpServer server;
    register_mix_tools(server);
    const auto graph = call_tool(server, "create_mix_graph", {{"part_ids", {1}}}, 50);
    // BS.1770 loudness and true peak are relative to full scale; neither exceeds 0.
    CHECK(call_tool(server,
                    "set_loudness_target",
                    {{"graph_id", graph["graph_id"]}, {"integrated_lufs", 5.0}},
                    51)
              .contains("error"));
    CHECK(
        call_tool(
            server,
            "set_loudness_target",
            {{"graph_id", graph["graph_id"]}, {"integrated_lufs", -14.0}, {"true_peak_dbfs", 3.0}},
            52)
            .contains("error"));
    CHECK(succeeded(call_tool(
        server,
        "set_loudness_target",
        {{"graph_id", graph["graph_id"]}, {"integrated_lufs", -14.0}, {"true_peak_dbfs", -1.0}},
        53)));

    const auto aux = call_tool(
        server, "create_aux_bus", {{"graph_id", graph["graph_id"]}, {"name", "Reverb"}}, 54);
    CHECK(call_tool(server,
                    "set_channel_send",
                    {{"graph_id", graph["graph_id"]},
                     {"channel_id", 1},
                     {"aux_id", aux["aux_bus_id"]},
                     {"level_db", 200.0}},
                    55)
              .contains("error"));
}

// =============================================================================
// Ableton plan-time refusal (#7)
// =============================================================================

TEST_CASE("compile_mix refuses a fader above Live's +6 dB ceiling before any mutation",
          "[mcp][mix][ableton][regression]") {
    CommandBuffer transport;
    McpServer server;
    register_mix_tools(server, &transport);
    const auto graph = call_tool(server, "create_mix_graph", {{"part_ids", {1}}}, 60);
    REQUIRE(
        succeeded(call_tool(server,
                            "set_channel_level",
                            {{"graph_id", graph["graph_id"]}, {"channel_id", 1}, {"level_db", 9.0}},
                            61)));
    const auto compiled =
        call_tool(server, "compile_mix", {{"graph_id", graph["graph_id"]}, {"base_track", 0}}, 62);
    INFO(compiled.dump());
    CHECK(compiled.contains("error"));
    CHECK(compiled["error_code"] == 4112); // TargetValueUnrepresentable
    CHECK(transport.size() == 0);
}

TEST_CASE("compile_mix refuses a send above Live's 0 dB ceiling before any mutation",
          "[mcp][mix][ableton][regression]") {
    CommandBuffer transport;
    McpServer server;
    register_mix_tools(server, &transport);
    const auto graph = call_tool(server, "create_mix_graph", {{"part_ids", {1}}}, 70);
    const auto aux = call_tool(
        server, "create_aux_bus", {{"graph_id", graph["graph_id"]}, {"name", "Reverb"}}, 71);
    REQUIRE(succeeded(call_tool(server,
                                "set_channel_send",
                                {{"graph_id", graph["graph_id"]},
                                 {"channel_id", 1},
                                 {"aux_id", aux["aux_bus_id"]},
                                 {"level_db", 3.0}},
                                72)));
    const auto compiled =
        call_tool(server, "compile_mix", {{"graph_id", graph["graph_id"]}, {"base_track", 0}}, 73);
    INFO(compiled.dump());
    CHECK(compiled.contains("error"));
    CHECK(compiled["error_code"] == 4112);
    CHECK(transport.size() == 0);
}

TEST_CASE("compile_timbre never plans a Dry/Wet write on EQ Eight",
          "[mcp][timbre][ableton][regression]") {
    CommandBuffer transport;
    McpServer server;
    register_timbre_tools(server, &transport);
    const auto profile =
        call_tool(server, "create_timbre_profile", {{"part_id", 1}, {"name", "EQ"}}, 80);
    REQUIRE(succeeded(
        call_tool(server,
                  "add_effect",
                  {{"profile_id", profile["profile_id"]}, {"effect_type", "eq"}, {"mix", 0.5}},
                  81)));
    REQUIRE(call_tool(
                server, "validate_timbre", {{"profile_id", profile["profile_id"]}}, 82)["valid"] ==
            true);
    const auto compiled = call_tool(
        server, "compile_timbre", {{"profile_id", profile["profile_id"]}, {"track_index", 0}}, 83);
    INFO(compiled.dump());
    REQUIRE(succeeded(compiled));
    CHECK_FALSE(any_command_mentions(transport, "Dry/Wet"));
    CHECK(any_warning_contains(compiled, "EQ Eight"));
    CHECK(compiled["complete"] == false);
}

// =============================================================================
// Timbre (#14)
// =============================================================================

struct TimbreFixture {
    std::shared_ptr<TimbreSession> session = std::make_shared<TimbreSession>();
    McpServer server;
    std::uint64_t profile_id = 0;

    TimbreFixture() {
        register_timbre_tools(server, nullptr, session);
        profile_id =
            call_tool(
                server, "create_timbre_profile", {{"part_id", 1}, {"name", "T"}}, 1)["profile_id"]
                .get<std::uint64_t>();
    }

    std::uint64_t add_delay(float feedback, int id) {
        const auto added = call_tool(
            server,
            "add_effect",
            {{"profile_id", profile_id}, {"effect_type", "delay"}, {"feedback", feedback}},
            id);
        REQUIRE(succeeded(added));
        return added["effect_id"].get<std::uint64_t>();
    }

    void route_lfo_to(const std::string& target, int id) {
        REQUIRE(succeeded(
            call_tool(server, "create_modulation_lfo", {{"profile_id", profile_id}}, id)));
        REQUIRE(succeeded(call_tool(server,
                                    "add_modulation",
                                    {{"profile_id", profile_id},
                                     {"source_type", 0},
                                     {"source_index", 0},
                                     {"target", target},
                                     {"depth", 0.5}},
                                    id + 1)));
    }

    float parameter(const std::string& path, int id) {
        const auto read =
            call_tool(server, "get_parameter", {{"profile_id", profile_id}, {"path", path}}, id);
        REQUIRE(read.contains("value"));
        return read["value"].get<float>();
    }

    bool valid(int id) {
        return call_tool(server, "validate_timbre", {{"profile_id", profile_id}}, id)["valid"] ==
               true;
    }
};

TEST_CASE("a modulation routing follows its effect through reorder_effects",
          "[mcp][timbre][modulation][regression]") {
    TimbreFixture fixture;
    const auto first = fixture.add_delay(0.3f, 100);
    const auto second = fixture.add_delay(0.7f, 101);
    fixture.route_lfo_to("insert_chain.effects[0].feedback", 102);

    REQUIRE(
        succeeded(call_tool(fixture.server,
                            "reorder_effects",
                            {{"profile_id", fixture.profile_id}, {"effect_ids", {second, first}}},
                            104)));
    const auto& routing = fixture.session->find(fixture.profile_id)->modulation.routings.at(0);
    CHECK(fixture.parameter(routing.target, 105) == Catch::Approx(0.3f));
    CHECK(fixture.valid(106));
}

TEST_CASE("removing an effect never leaves a dangling modulation reference",
          "[mcp][timbre][modulation][regression]") {
    TimbreFixture fixture;
    const auto first = fixture.add_delay(0.3f, 110);
    const auto second = fixture.add_delay(0.7f, 111);
    fixture.route_lfo_to("insert_chain.effects[1].feedback", 112);

    // The routed effect is referenced, so its removal is refused.
    CHECK(call_tool(fixture.server,
                    "remove_effect",
                    {{"profile_id", fixture.profile_id}, {"effect_id", second}},
                    114)
              .contains("error"));
    CHECK(fixture.valid(115));
    CHECK(fixture.session->find(fixture.profile_id)->insert_chain.effects.size() == 2);

    // Removing the unreferenced effect before it shifts the routed effect down.
    REQUIRE(succeeded(call_tool(fixture.server,
                                "remove_effect",
                                {{"profile_id", fixture.profile_id}, {"effect_id", first}},
                                116)));
    const auto& routing = fixture.session->find(fixture.profile_id)->modulation.routings.at(0);
    CHECK(fixture.parameter(routing.target, 117) == Catch::Approx(0.7f));
    CHECK(fixture.valid(118));
}

TEST_CASE("a failed load_preset leaves the profile unchanged",
          "[mcp][timbre][preset][regression]") {
    TimbreFixture fixture;
    const auto donor = call_tool(fixture.server,
                                 "create_timbre_profile",
                                 {{"part_id", 2}, {"name", "D"}},
                                 120)["profile_id"];
    REQUIRE(succeeded(
        call_tool(fixture.server,
                  "set_sound_source",
                  {{"profile_id", donor}, {"source_type", "subtractive"}, {"oscillator_count", 2}},
                  121)));
    REQUIRE(succeeded(call_tool(
        fixture.server,
        "set_parameter",
        {{"profile_id", donor}, {"path", "source.amplifier.velocity_sensitivity"}, {"value", 0.9}},
        122)));
    const auto preset = call_tool(
        fixture.server, "save_preset", {{"profile_id", donor}, {"name", "Two oscillators"}}, 123);

    REQUIRE(succeeded(call_tool(fixture.server,
                                "set_parameter",
                                {{"profile_id", fixture.profile_id},
                                 {"path", "source.amplifier.velocity_sensitivity"},
                                 {"value", 0.1}},
                                124)));
    // The one-oscillator profile cannot resolve source.oscillators[1].
    CHECK(call_tool(fixture.server,
                    "load_preset",
                    {{"profile_id", fixture.profile_id}, {"preset_id", preset["preset_id"]}},
                    125)
              .contains("error"));
    CHECK(fixture.parameter("source.amplifier.velocity_sensitivity", 126) == Catch::Approx(0.1f));
}

TEST_CASE("compile_timbre warns for each modulation routing and preset morph it drops",
          "[mcp][timbre][ableton][regression]") {
    CommandBuffer transport;
    auto session = std::make_shared<TimbreSession>();
    McpServer server;
    register_timbre_tools(server, &transport, session);
    const auto profile =
        call_tool(server, "create_timbre_profile", {{"part_id", 1}, {"name", "Mod"}}, 130);
    REQUIRE(succeeded(
        call_tool(server, "create_modulation_lfo", {{"profile_id", profile["profile_id"]}}, 131)));
    REQUIRE(succeeded(call_tool(server,
                                "add_modulation",
                                {{"profile_id", profile["profile_id"]},
                                 {"source_type", 0},
                                 {"source_index", 0},
                                 {"target", "source.filter.cutoff"},
                                 {"depth", 0.5}},
                                132)));
    const auto preset = call_tool(
        server, "save_preset", {{"profile_id", profile["profile_id"]}, {"name", "P"}}, 133);
    REQUIRE(succeeded(call_tool(server,
                                "morph_presets",
                                {{"profile_id", profile["profile_id"]},
                                 {"from_preset_id", preset["preset_id"]},
                                 {"to_preset_id", preset["preset_id"]},
                                 {"start_bar", 1},
                                 {"end_bar", 2}},
                                134)));
    const auto compiled = call_tool(
        server, "compile_timbre", {{"profile_id", profile["profile_id"]}, {"track_index", 0}}, 135);
    INFO(compiled.dump());
    REQUIRE(succeeded(compiled));
    CHECK(any_warning_contains(compiled, "source.filter.cutoff"));
    CHECK(any_warning_contains(compiled, "morph"));
    CHECK(compiled["complete"] == false);
}

TEST_CASE("morph_presets rejects unknown presets and times before the score start",
          "[mcp][timbre][preset][regression]") {
    TimbreFixture fixture;
    const auto preset = call_tool(
        fixture.server, "save_preset", {{"profile_id", fixture.profile_id}, {"name", "P"}}, 140);
    const auto morph = [&](std::uint64_t from, std::uint64_t to, int start, int end, int id) {
        return call_tool(fixture.server,
                         "morph_presets",
                         {{"profile_id", fixture.profile_id},
                          {"from_preset_id", from},
                          {"to_preset_id", to},
                          {"start_bar", start},
                          {"end_bar", end}},
                         id);
    };
    const auto known = preset["preset_id"].get<std::uint64_t>();
    CHECK(morph(known, 999, 1, 2, 141).contains("error"));
    CHECK(morph(999, known, 1, 2, 142).contains("error"));
    CHECK(morph(known, known, 0, 2, 143).contains("error"));
    CHECK(fixture.session->find(fixture.profile_id)->preset_morphs.empty());
    CHECK(succeeded(morph(known, known, 1, 2, 144)));
}

} // namespace
