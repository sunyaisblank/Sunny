/** Public measure edits relocate real sibling controls and retain one atomic history. */
#include <catch2/catch_test_macros.hpp>
#include <sunny/infrastructure/mcp/mix_tools.hpp>
#include <sunny/infrastructure/mcp/project_tools.hpp>
#include <sunny/infrastructure/mcp/score_tools.hpp>
#include <sunny/infrastructure/mcp/timbre_tools.hpp>

using namespace sunny::infrastructure;
using namespace sunny::core;
using json = nlohmann::json;

namespace {
struct TimelineFixture {
    McpServer server;
    McpSession session;
    TimelineFixture() {
        register_score_tools(server, session.score);
        register_timbre_tools(server, session.timbre);
        register_mix_tools(server, session.mix);
        register_project_tools(server, session);
        REQUIRE_FALSE(
            call("score_create",
                 {{"total_bars", 5},
                  {"parts",
                   {{{"name", "Voice"},
                     {"instrument_type", static_cast<int>(InstrumentType::Synthesiser)}}}}})
                .contains("error"));
        REQUIRE_FALSE(call("create_project", {{"score_id", 1}}).contains("error"));
    }
    json call(const std::string& name, const json& arguments) {
        const auto reply =
            server.process_request({{"jsonrpc", "2.0"},
                                    {"id", 1},
                                    {"method", "tools/call"},
                                    {"params", {{"name", name}, {"arguments", arguments}}}});
        INFO(name << ": " << reply.dump());
        REQUIRE(reply.contains("result"));
        return json::parse(reply["result"]["content"][0]["text"].get<std::string>());
    }
    json state() { return call("get_project_json", {{"score_id", 1}}); }
};
} // namespace

TEST_CASE("MCP measure splice relocates automation and clips morphs at exact surviving bar anchors",
          "[mcp][project][timeline]") {
    TimelineFixture fixture;
    REQUIRE_FALSE(fixture
                      .call("add_automation",
                            {{"profile_id", 1},
                             {"path", "source.filter.cutoff"},
                             {"interpolation", 0},
                             {"breakpoints",
                              {{{"bar", 1}, {"value", 1800}},
                               {{"bar", 3}, {"beat_num", 1}, {"beat_den", 4}, {"value", 4200}},
                               {{"bar", 6}, {"value", 1800}}}}})
                      .contains("error"));
    REQUIRE_FALSE(fixture
                      .call("add_mix_automation",
                            {{"graph_id", 1},
                             {"target", "channels[1].fader.level_db"},
                             {"interpolation", 0},
                             {"breakpoints",
                              {{{"bar", 2}, {"value", -9}},
                               {{"bar", 4}, {"beat_num", 1}, {"beat_den", 2}, {"value", -12}}}}})
                      .contains("error"));
    const auto first =
        fixture.call("save_preset", {{"profile_id", 1}, {"name", "First"}})["preset_id"];
    REQUIRE_FALSE(fixture
                      .call("set_parameter",
                            {{"profile_id", 1}, {"path", "source.filter.cutoff"}, {"value", 4200}})
                      .contains("error"));
    const auto second =
        fixture.call("save_preset", {{"profile_id", 1}, {"name", "Second"}})["preset_id"];
    REQUIRE_FALSE(fixture
                      .call("morph_presets",
                            {{"profile_id", 1},
                             {"from_preset_id", first},
                             {"to_preset_id", second},
                             {"start_bar", 2},
                             {"end_bar", 5}})
                      .contains("error"));

    const auto inserted =
        fixture.call("score_insert_measures", {{"score_id", 1}, {"after_bar", 1}, {"count", 2}});
    REQUIRE_FALSE(inserted.contains("error"));
    CHECK(inserted["control_relocation"]["points_or_endpoints_relocated"] == 6);
    CHECK(fixture.session.score->find(1)->metadata.total_bars == 7);
    const auto& automation = fixture.session.timbre->find(1)->parameter_automation[0];
    CHECK(automation.breakpoints[1].time == ScoreTime{5, Beat{1, 4}});
    CHECK(automation.breakpoints[2].time == ScoreTime{8, Beat::zero()});
    CHECK(fixture.session.mix->find(1)->automation[0].breakpoints[1].time ==
          ScoreTime{6, Beat{1, 2}});
    CHECK(fixture.session.timbre->find(1)->preset_morphs[0].start == ScoreTime{4, Beat::zero()});
    CHECK(fixture.session.timbre->find(1)->preset_morphs[0].end == ScoreTime{7, Beat::zero()});
    REQUIRE_FALSE(fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.session.timbre->find(1)->parameter_automation[0].breakpoints[1].time ==
          ScoreTime{3, Beat{1, 4}});
    CHECK(fixture.session.mix->find(1)->automation[0].breakpoints[1].time ==
          ScoreTime{4, Beat{1, 2}});
    REQUIRE_FALSE(fixture.call("score_redo", {{"score_id", 1}}).contains("error"));

    const auto deleted =
        fixture.call("score_delete_measures", {{"score_id", 1}, {"bar", 4}, {"count", 2}});
    REQUIRE_FALSE(deleted.contains("error"));
    CHECK(deleted["control_relocation"]["points_removed"] == 2);
    CHECK(deleted["control_relocation"]["morph_endpoints_clipped"] == 1);
    CHECK(deleted["control_relocation"]["points_or_endpoints_relocated"] == 3);
    CHECK(fixture.session.timbre->find(1)->parameter_automation[0].breakpoints.size() == 2);
    CHECK(fixture.session.timbre->find(1)->parameter_automation[0].breakpoints[1].time ==
          ScoreTime{6, Beat::zero()});
    CHECK(fixture.session.mix->find(1)->automation[0].breakpoints[0].time ==
          ScoreTime{4, Beat{1, 2}});
    CHECK(fixture.session.timbre->find(1)->preset_morphs[0].start == ScoreTime{4, Beat::zero()});
    CHECK(fixture.session.timbre->find(1)->preset_morphs[0].end == ScoreTime{5, Beat::zero()});
    const auto collapsed =
        fixture.call("score_delete_measures", {{"score_id", 1}, {"bar", 4}, {"count", 2}});
    REQUIRE_FALSE(collapsed.contains("error"));
    CHECK(collapsed["control_relocation"]["empty_lanes_removed"] == 1);
    CHECK(collapsed["control_relocation"]["collapsed_morphs_removed"] == 1);
    CHECK(fixture.session.mix->find(1)->automation.empty());
    CHECK(fixture.session.timbre->find(1)->preset_morphs.empty());
    REQUIRE_FALSE(fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    REQUIRE(fixture.session.mix->find(1)->automation.size() == 1);
    REQUIRE(fixture.session.timbre->find(1)->preset_morphs.size() == 1);
}

TEST_CASE("Bound meter edits reject orphan control offsets without consuming state or history",
          "[mcp][project][timeline]") {
    TimelineFixture fixture;
    REQUIRE_FALSE(fixture
                      .call("add_automation",
                            {{"profile_id", 1},
                             {"path", "source.filter.cutoff"},
                             {"breakpoints",
                              {{{"bar", 1}, {"beat_num", 7}, {"beat_den", 8}, {"value", 1800}}}}})
                      .contains("error"));
    const auto before = fixture.state();
    const auto rejected =
        fixture.call("score_set_time_signature",
                     {{"score_id", 1}, {"bar", 1}, {"groups", {3}}, {"denominator", 4}});
    REQUIRE(rejected.contains("error"));
    CHECK(fixture.state() == before);
    const auto outside = fixture.call("add_mix_automation",
                                      {{"graph_id", 1},
                                       {"target", "channels[1].fader.level_db"},
                                       {"breakpoints", {{{"bar", 7}, {"value", -9}}}}});
    CHECK(outside.contains("error"));
    CHECK(fixture.state() == before);
}

TEST_CASE("MCP compose inspect revise loop edits independent voices key meter and hairpin",
          "[mcp][score][structure]") {
    TimelineFixture fixture;
    REQUIRE_FALSE(fixture
                      .call("score_add_voice",
                            {{"score_id", 1}, {"part_id", 1}, {"bar", 1}, {"voice_index", 2}})
                      .contains("error"));
    REQUIRE_FALSE(fixture
                      .call("score_insert_note",
                            {{"score_id", 1},
                             {"part_id", 1},
                             {"bar", 1},
                             {"voice", 2},
                             {"pitch", {{"letter", "G"}, {"accidental", 0}, {"octave", 4}}},
                             {"duration", {{"n", 1}, {"d", 4}}}})
                      .contains("error"));
    REQUIRE_FALSE(fixture
                      .call("score_set_key_signature",
                            {{"score_id", 1},
                             {"position", {{"bar", 2}, {"beat_n", 0}, {"beat_d", 1}}},
                             {"root", {{"letter", "G"}, {"accidental", 0}, {"octave", 4}}},
                             {"mode", "dorian"}})
                      .contains("error"));
    CHECK(fixture.session.score->find(1)->key_map.back().key.accidentals == -1);
    REQUIRE_FALSE(
        fixture
            .call("score_set_time_signature",
                  {{"score_id", 1}, {"bar", 3}, {"groups", {3, 2, 2}}, {"denominator", 8}})
            .contains("error"));
    CHECK(fixture.session.score->find(1)->time_map.back().time_signature.measure_duration() ==
          Beat{7, 8});
    REQUIRE_FALSE(fixture
                      .call("score_insert_hairpin",
                            {{"score_id", 1},
                             {"part_id", 1},
                             {"type", 0},
                             {"start", {{"bar", 1}, {"beat_n", 0}, {"beat_d", 1}}},
                             {"end", {{"bar", 3}, {"beat_n", 0}, {"beat_d", 1}}}})
                      .contains("error"));
    const auto inspected = fixture.call("score_get_json", {{"score_id", 1}});
    CHECK(inspected["parts"][0]["hairpins"].size() == 1);
    REQUIRE_FALSE(fixture
                      .call("score_remove_voice",
                            {{"score_id", 1}, {"part_id", 1}, {"bar", 1}, {"voice_index", 2}})
                      .contains("error"));
    CHECK(fixture.session.score->find(1)->parts[0].measures[0].voices.size() == 1);
    REQUIRE_FALSE(fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.session.score->find(1)->parts[0].measures[0].voices.size() == 2);
    const auto before = fixture.state();
    CHECK(fixture
              .call("score_add_voice",
                    {{"score_id", 1}, {"part_id", 1}, {"bar", 1}, {"voice_index", 256}})
              .contains("error"));
    CHECK(fixture
              .call("score_set_time_signature",
                    {{"score_id", 1}, {"bar", 3}, {"groups", {2, 0, 3}}, {"denominator", 8}})
              .contains("error"));
    CHECK(fixture.state() == before);
}
