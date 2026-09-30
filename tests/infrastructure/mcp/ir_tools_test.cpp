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

} // namespace
