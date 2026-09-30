/**
 * @file score_tools_test.cpp
 * @brief Score MCP tool contract tests
 *
 * Each test drives the registered tools through the JSON-RPC request seam
 * and states the postcondition a client observes. Expected values are
 * derived from the MIDI, MusicXML and Score IR specifications rather than
 * from the implementation.
 */

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <set>
#include <string>
#include <sunny/infrastructure/mcp/score_tools.hpp>
#include <sunny/infrastructure/mcp/server.hpp>

using namespace sunny::infrastructure;
using json = nlohmann::json;

namespace {

json call_tool(McpServer& server, const std::string& name, const json& arguments) {
    static int request_id = 50'000;
    auto response = server.process_request({{"jsonrpc", "2.0"},
                                            {"method", "tools/call"},
                                            {"params", {{"name", name}, {"arguments", arguments}}},
                                            {"id", ++request_id}});
    INFO("tool response for " << name << ": " << response.dump());
    REQUIRE(response.contains("result"));
    return json::parse(response["result"]["content"][0]["text"].get<std::string>());
}

struct ScoreFixture {
    McpServer server;
    std::shared_ptr<ScoreSession> session = std::make_shared<ScoreSession>();

    ScoreFixture() { register_score_tools(server, nullptr, session); }

    json call(const std::string& name, const json& arguments) {
        return call_tool(server, name, arguments);
    }
};

json pitch(const char* letter, int accidental, int octave) {
    return {{"letter", letter}, {"accidental", accidental}, {"octave", octave}};
}

} // anonymous namespace

// =============================================================================
// Issue #9: distinct default MIDI channels
// =============================================================================

TEST_CASE("parts created through MCP receive distinct channels, skipping 10 for pitched parts",
          "[mcp][score][midi][regression]") {
    // Eleven pitched parts take channels 1..9, 11, 12 (General MIDI reserves 10
    // for percussion key maps); a snare drum (instrument 39) takes channel 10.
    ScoreFixture fixture;
    json parts = json::array();
    for (int index = 0; index < 11; ++index)
        parts.push_back({{"name", "P" + std::to_string(index)}, {"instrument_type", 0}});
    parts.push_back({{"name", "Snare"}, {"instrument_type", 39}});
    const auto created = fixture.call("score_create", {{"total_bars", 1}, {"parts", parts}});
    REQUIRE(created.contains("score_id"));

    const auto document = fixture.call("score_get_json", {{"score_id", created["score_id"]}});
    std::vector<int> channels;
    for (const auto& part : document["parts"])
        channels.push_back(part["definition"]["rendering"]["midi_channel"].get<int>());
    CHECK(channels == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 10});

    const auto added =
        fixture.call("score_add_part",
                     {{"score_id", created["score_id"]}, {"name", "Late"}, {"instrument_type", 0}});
    REQUIRE(added.contains("part_id"));
    const auto after = fixture.call("score_get_json", {{"score_id", created["score_id"]}});
    CHECK(after["parts"].back()["definition"]["rendering"]["midi_channel"] == 13);
}

TEST_CASE("an MCP two-part unison compiles to one note per part on separate channels",
          "[mcp][score][midi][regression]") {
    // Part A holds a whole C4 and part B a quarter C4 at the same onset. On
    // distinct channels both attacks survive: channel 1 [0, 1920) and
    // channel 2 [0, 480) at PPQ 480.
    ScoreFixture fixture;
    const auto created = fixture.call("score_create",
                                      {{"total_bars", 1},
                                       {"parts",
                                        json::array({{{"name", "A"}, {"instrument_type", 47}},
                                                     {{"name", "B"}, {"instrument_type", 47}}})}});
    const auto score_id = created["score_id"];
    const auto part_a = created["part_ids"][0];
    const auto part_b = created["part_ids"][1];
    REQUIRE(fixture
                .call("score_insert_note",
                      {{"score_id", score_id},
                       {"part_id", part_a},
                       {"bar", 1},
                       {"pitch", pitch("C", 0, 4)},
                       {"duration", {{"n", 1}, {"d", 1}}}})
                .contains("ok"));
    REQUIRE(fixture
                .call("score_insert_note",
                      {{"score_id", score_id},
                       {"part_id", part_b},
                       {"bar", 1},
                       {"pitch", pitch("C", 0, 4)},
                       {"duration", {{"n", 1}, {"d", 4}}}})
                .contains("ok"));

    const auto midi = fixture.call("score_compile_to_midi", {{"score_id", score_id}});
    REQUIRE(midi["notes"].size() == 2);
    std::set<std::tuple<int, int, int>> notes;
    for (const auto& note : midi["notes"])
        notes.insert({note["channel"].get<int>(),
                      note["tick"].get<int>(),
                      note["duration_ticks"].get<int>()});
    CHECK(notes == std::set<std::tuple<int, int, int>>{{1, 0, 1920}, {2, 0, 480}});
}
