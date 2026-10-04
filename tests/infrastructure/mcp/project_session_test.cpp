/** Bound project workflows through the public MCP request seam. */
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <stdexcept>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/mix/workflows.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/timbre/serialization.hpp>
#include <sunny/core/timbre/workflows.hpp>
#include <sunny/infrastructure/mcp/mix_tools.hpp>
#include <sunny/infrastructure/mcp/project_tools.hpp>
#include <sunny/infrastructure/mcp/score_tools.hpp>
#include <sunny/infrastructure/mcp/timbre_tools.hpp>

using namespace sunny::infrastructure;
using namespace sunny::core;
using json = nlohmann::json;

namespace {

struct ProjectFixture {
    McpServer server;
    McpSession session;
    int request_id = 0;

    explicit ProjectFixture(int parts = 1, bool inject_failures = false) {
        register_score_tools(server, session.score);
        register_timbre_tools(server, session.timbre);
        register_mix_tools(server, session.mix);
        if (inject_failures) {
            auto scope = server.registration_scope(McpDocumentDomain::Score);
            server.register_tool("reject_after_mutation",
                                 "Fault injection",
                                 {{"score_id", "integer"}},
                                 [this](const json&) -> json {
                                     session.score->find(1)->metadata.title = "Uncommitted";
                                     ++session.timbre->next_effect_id;
                                     return {{"error", "Injected refusal"}};
                                 });
            server.register_tool("throw_after_mutation",
                                 "Fault injection",
                                 {{"score_id", "integer"}},
                                 [this](const json&) -> json {
                                     session.mix->find(1)->channels[0].fader.level_db = -20;
                                     ++session.timbre->next_effect_id;
                                     throw std::runtime_error("Injected exception");
                                 });
            server.register_tool("json_error_after_mutation",
                                 "Fault injection",
                                 {{"score_id", "integer"}},
                                 [this](const json&) -> json {
                                     session.score->find(1)->metadata.title = "Uncommitted JSON";
                                     ++session.timbre->next_effect_id;
                                     return json::parse("{");
                                 });
            server.register_tool("encoding_error_after_mutation",
                                 "Fault injection",
                                 {{"score_id", "integer"}},
                                 [this](const json&) -> json {
                                     session.score->find(1)->metadata.title =
                                         std::string(1, static_cast<char>(0xff));
                                     ++session.timbre->next_effect_id;
                                     return {{"success", true}};
                                 });
            server.register_tool(
                "response_error_after_mutation",
                "Fault injection",
                {{"score_id", "integer"}},
                [this](const json&) -> json {
                    session.score->find(1)->metadata.title = "Uncommitted response";
                    ++session.timbre->next_effect_id;
                    return {{"success", true}, {"text", std::string(1, static_cast<char>(0xff))}};
                });
        }
        register_project_tools(server, session);
        create_score(parts);
        REQUIRE(!call("create_project", {{"score_id", 1}}).contains("error"));
    }

    json call(const std::string& name, const json& arguments) {
        const auto response =
            server.process_request({{"jsonrpc", "2.0"},
                                    {"id", ++request_id},
                                    {"method", "tools/call"},
                                    {"params", {{"name", name}, {"arguments", arguments}}}});
        INFO(name << ": " << response.dump());
        REQUIRE(response.contains("result"));
        return json::parse(response["result"]["content"][0]["text"].get<std::string>());
    }

    std::uint64_t create_score(int parts) {
        json definitions = json::array();
        for (int index = 0; index < parts; ++index)
            definitions.push_back(
                {{"name", "Part " + std::to_string(index + 1)}, {"instrument_type", 47}});
        const auto result =
            call("score_create", {{"title", "Fixture"}, {"total_bars", 2}, {"parts", definitions}});
        REQUIRE(!result.contains("error"));
        return result["score_id"].get<std::uint64_t>();
    }

    json state(std::uint64_t id = 1) { return call("get_project_json", {{"score_id", id}}); }
};

} // namespace

TEST_CASE("Project binding retains inactive revision and Channel namespaces",
          "[mcp][project][identity][workspace]") {
    ProjectFixture fixture;
    fixture.session.project->projects.clear();
    fixture.session.namespace_history->project_revision_floors.emplace(1, 8);
    fixture.session.namespace_history->graph_channels.emplace(1, std::set<std::uint64_t>{1, 2});
    const auto bound = fixture.call(
        "bind_project", {{"score_id", 1}, {"timbre_profile_ids", {1}}, {"mix_graph_id", 1}});
    REQUIRE_FALSE(bound.contains("error"));
    CHECK(bound.at("revision") == 9);
    const auto added = fixture.call("score_add_part", {{"score_id", 1}, {"name", "New part"}});
    REQUIRE_FALSE(added.contains("error"));
    // Channel2 was previously exposed under graph1, even though no active
    // channel or project binding retained it at this rebind.
    CHECK(fixture.session.mix->find(1)->channels.back().id == ChannelStripId{3});
}

TEST_CASE("Project creation and binding refuse inactive revision exhaustion atomically",
          "[mcp][project][identity][workspace][atomicity]") {
    for (const bool bind_existing : {false, true}) {
        ProjectFixture fixture;
        fixture.session.project->projects.clear();
        fixture.session.namespace_history->project_revision_floors.emplace(
            1, std::numeric_limits<std::uint64_t>::max());
        const auto next_profile = fixture.session.timbre->next_profile_id;
        const auto next_graph = fixture.session.mix->next_graph_id;
        const auto result =
            bind_existing
                ? fixture.call("bind_project",
                               {{"score_id", 1}, {"timbre_profile_ids", {1}}, {"mix_graph_id", 1}})
                : fixture.call("create_project", {{"score_id", 1}});
        CHECK(result.contains("error"));
        CHECK(fixture.session.project->projects.empty());
        CHECK(fixture.session.timbre->profiles.size() == 1);
        CHECK(fixture.session.mix->graphs.size() == 1);
        CHECK(fixture.session.timbre->next_profile_id == next_profile);
        CHECK(fixture.session.mix->next_graph_id == next_graph);
    }
}

TEST_CASE("Creating siblings uses the retained project publication floor",
          "[mcp][project][identity][workspace]") {
    ProjectFixture fixture;
    fixture.session.project->projects.clear();
    fixture.session.namespace_history->project_revision_floors.emplace(1, 8);
    const auto created = fixture.call("create_project", {{"score_id", 1}});
    REQUIRE_FALSE(created.contains("error"));
    CHECK(created.at("revision") == 9);
}

TEST_CASE("Project sibling creation observes complete global identity domains",
          "[mcp][project][identity][allocation]") {
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    ProjectFixture fixture;
    fixture.session.timbre->profiles.emplace(
        73, create_timbre_profile(TimbreProfileId{73}, PartId{1}, "Imported"));
    fixture.session.timbre->next_profile_id = 70;
    const auto before = fixture.state();
    CHECK(
        fixture.call("score_add_part", {{"score_id", 1}, {"name", "Rejected"}}).contains("error"));
    CHECK(fixture.state() == before);
    CHECK(fixture.session.timbre->next_profile_id == 70);
    const auto score_id = fixture.create_score(2);
    CHECK(fixture.call("create_project", {{"score_id", score_id}}).contains("error"));
    CHECK_FALSE(fixture.session.project->projects.contains(score_id));
    fixture.session.timbre->next_profile_id = 74;
    auto imported_graph = create_mix_graph(MixGraphId{73}, {PartId{1}});
    fixture.session.mix->graphs.emplace(73, std::move(imported_graph));
    fixture.session.mix->next_graph_id = 70;
    CHECK(fixture.call("create_project", {{"score_id", score_id}}).contains("error"));
    CHECK(fixture.session.timbre->next_profile_id == 74);
    CHECK_FALSE(fixture.session.timbre->profiles.contains(74));
    fixture.session.mix->next_graph_id = 74;
    fixture.session.timbre->next_profile_id = maximum - 1;
    // Two Parts require a complete two-ID range; no first sibling is published.
    CHECK(fixture.call("create_project", {{"score_id", score_id}}).contains("error"));
    CHECK(fixture.session.timbre->next_profile_id == maximum - 1);
    CHECK(fixture.session.mix->next_graph_id == 74);
    CHECK_FALSE(fixture.session.timbre->profiles.contains(maximum - 1));
    fixture.session.timbre->next_profile_id = 74;
    const auto created = fixture.call("create_project", {{"score_id", score_id}});
    REQUIRE_FALSE(created.contains("error"));
    CHECK(created.at("timbre_profile_ids") == json::array({74, 75}));
    CHECK(created.at("mix_graph_id") == 74);
    CHECK(fixture.session.timbre->next_profile_id == 76);
    CHECK(fixture.session.mix->next_graph_id == 75);
}

TEST_CASE("Deployment plan admission rejects invalid identities without pruning state",
          "[mcp][project][identity][plan]") {
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    ProjectFixture fixture;
    const json arguments{{"score_id", 1}, {"timbre_profile_ids", {1}}, {"mix_graph_id", 1}};
    for (std::uint64_t id = 1; id <= 257; ++id) {
        StoredProjectDeploymentPlan retained;
        retained.consumed = id == 1;
        fixture.session.deployment->plans.emplace(id, std::move(retained));
    }
    for (const auto next : {std::uint64_t{0}, std::uint64_t{70}, maximum}) {
        fixture.session.deployment->next_plan_id = next;
        const auto rejected = fixture.call("project_plan_to_ableton", arguments);
        REQUIRE(rejected.contains("error"));
        CHECK(rejected.contains("error_code"));
        CHECK(fixture.session.deployment->next_plan_id == next);
        CHECK(fixture.session.deployment->plans.size() == 257);
        CHECK(fixture.session.deployment->plans.at(1).consumed);
    }
    fixture.session.deployment->next_plan_id = 300;
    CHECK(fixture.call("project_plan_to_ableton", arguments).contains("error"));
    CHECK(fixture.session.deployment->plans.size() == 257);
    CHECK(fixture.session.deployment->plans.contains(1));
    CHECK(fixture.session.deployment->next_plan_id == 300);
}

TEST_CASE("Bound old tools share project history and Part construction closes every sibling",
          "[mcp][project][authoring]") {
    ProjectFixture fixture;
    REQUIRE(!fixture
                 .call("set_parameter",
                       {{"profile_id", 1}, {"path", "source.filter.cutoff"}, {"value", 4200}})
                 .contains("error"));
    REQUIRE(
        !fixture.call("set_channel_level", {{"graph_id", 1}, {"channel_id", 1}, {"level_db", -9}})
             .contains("error"));
    auto added = fixture.call("score_add_part", {{"score_id", 1}, {"name", "Second"}});
    REQUIRE(!added.contains("error"));
    CHECK(added["part_id"] == 2);
    CHECK(added["timbre_profile_id"] == 2);
    CHECK(added["channel_id"] == 2);
    CHECK(fixture.session.score->find(1)->parts.size() == 2);
    CHECK(fixture.session.timbre->find(2)->part_id == PartId{2});
    CHECK(fixture.session.mix->find(1)->channels[1].part_id == PartId{2});
    CHECK(fixture.session.project->projects.at(1).undo_entries.size() == 3);
    CHECK(fixture.session.score->undo_stacks.empty());
    const auto version = fixture.session.score->find(1)->version;
    REQUIRE(!fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.session.score->find(1)->version > version);
    CHECK(fixture.session.score->find(1)->parts.size() == 1);
    CHECK(fixture.session.timbre->find(2) == nullptr);
    CHECK(fixture.session.mix->find(1)->channels.size() == 1);
    CHECK(*get_parameter(*fixture.session.timbre->find(1), "source.filter.cutoff") ==
          Catch::Approx(4200));
    CHECK(fixture.session.mix->find(1)->channels[0].fader.level_db == Catch::Approx(-9));
    CHECK(fixture.session.timbre->next_profile_id == 3);
    REQUIRE(!fixture.call("score_redo", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.session.score->find(1)->parts[1].id == PartId{2});
    CHECK(fixture.session.timbre->find(2)->part_id == PartId{2});
    CHECK(fixture.session.mix->find(1)->channels[1].id == ChannelStripId{2});
    REQUIRE(!fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    const auto replacement =
        fixture.call("score_add_part", {{"score_id", 1}, {"name", "Replacement"}});
    REQUIRE(!replacement.contains("error"));
    CHECK(replacement["part_id"] == 3);
    CHECK(replacement["timbre_profile_id"] == 3);
    CHECK(replacement["channel_id"] == 3);
    CHECK(fixture.call("score_redo", {{"score_id", 1}}).contains("error"));
}

TEST_CASE("Every bound domain rejects failures without consuming state counters or history",
          "[mcp][project][authoring]") {
    ProjectFixture fixture;
    const auto before = fixture.state();
    const auto profile_id = fixture.session.timbre->next_profile_id;
    const auto effect_id = fixture.session.timbre->next_effect_id;
    const auto mix_effect_id = fixture.session.mix->next_effect_id;
    CHECK(fixture
              .call("set_parameter",
                    {{"profile_id", 1}, {"path", "source.filter.cutoff"}, {"value", -100}})
              .contains("error"));
    CHECK(fixture
              .call("add_channel_effect",
                    {{"graph_id", 1},
                     {"channel_id", 1},
                     {"effect_type", "compressor"},
                     {"ratio", 0.25}})
              .contains("error"));
    CHECK(fixture.call("score_remove_part", {{"score_id", 1}, {"part_id", 1}}).contains("error"));
    // Locally admitted AudioFollower must still name a Part in this exact Score.
    CHECK(fixture
              .call("add_modulation",
                    {{"profile_id", 1},
                     {"source_type", static_cast<int>(ModulationSourceType::AudioFollower)},
                     {"source_sidechain_part", 99},
                     {"target", "source.filter.cutoff"},
                     {"depth", 0.5}})
              .contains("error"));
    CHECK(fixture.state() == before);
    CHECK(fixture.session.timbre->next_profile_id == profile_id);
    CHECK(fixture.session.timbre->next_effect_id == effect_id);
    CHECK(fixture.session.mix->next_effect_id == mix_effect_id);
    CHECK(fixture.session.project->projects.at(1).undo_entries.empty());
}

TEST_CASE("Bound handler errors and exceptions restore candidates before the next request",
          "[mcp][project][authoring]") {
    ProjectFixture fixture(1, true);
    const auto before = fixture.state();
    const auto counter = fixture.session.timbre->next_effect_id;
    CHECK(fixture.call("reject_after_mutation", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.call("throw_after_mutation", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.call("json_error_after_mutation", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.call("encoding_error_after_mutation", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.call("response_error_after_mutation", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.state() == before);
    CHECK(fixture.session.timbre->next_effect_id == counter);
    CHECK_FALSE(fixture.session.score->project_transaction_score.has_value());
}

TEST_CASE("Project membership is exclusive and identical local Part IDs do not imply ownership",
          "[mcp][project][authoring]") {
    ProjectFixture fixture;
    const auto second_score = fixture.create_score(1);
    CHECK(fixture.session.score->find(second_score)->parts[0].id == PartId{1});
    const auto scratch =
        fixture.call("create_timbre_profile", {{"part_id", 1}, {"name", "Scratch"}});
    const auto scratch_id = scratch["profile_id"].get<std::uint64_t>();
    const auto graph = fixture.call("create_mix_graph", {{"part_ids", {1}}});
    const auto graph_id = graph["graph_id"].get<std::uint64_t>();
    CHECK(fixture
              .call("bind_project",
                    {{"score_id", second_score},
                     {"timbre_profile_ids", {1}},
                     {"mix_graph_id", graph_id}})
              .contains("error"));
    CHECK(fixture.session.project->projects.size() == 1);
    REQUIRE(
        !fixture
             .call("set_parameter",
                   {{"profile_id", scratch_id}, {"path", "source.filter.cutoff"}, {"value", 6000}})
             .contains("error"));
    CHECK(fixture.session.project->projects.at(1).undo_entries.empty());
    REQUIRE(!fixture
                 .call("bind_project",
                       {{"score_id", second_score},
                        {"timbre_profile_ids", {scratch_id}},
                        {"mix_graph_id", graph_id}})
                 .contains("error"));
    REQUIRE(
        !fixture
             .call("set_parameter",
                   {{"profile_id", scratch_id}, {"path", "source.filter.cutoff"}, {"value", 7000}})
             .contains("error"));
    CHECK(fixture.session.project->projects.at(1).undo_entries.empty());
    CHECK(fixture.session.project->projects.at(second_score).undo_entries.size() == 1);
    REQUIRE(!fixture.call("score_undo", {{"score_id", second_score}}).contains("error"));
    CHECK(*get_parameter(*fixture.session.timbre->find(scratch_id), "source.filter.cutoff") ==
          Catch::Approx(6000));
}

TEST_CASE("Part removal refuses retained references and history restores configured siblings",
          "[mcp][project][authoring]") {
    ProjectFixture fixture(2);
    REQUIRE(!fixture
                 .call("set_sound_source",
                       {{"profile_id", 2}, {"source_type", "fm"}, {"feedback", 0.25}})
                 .contains("error"));
    REQUIRE(
        !fixture.call("set_channel_level", {{"graph_id", 1}, {"channel_id", 2}, {"level_db", -12}})
             .contains("error"));
    REQUIRE(!fixture
                 .call("create_group_bus",
                       {{"graph_id", 1}, {"name", "Group"}, {"member_channel_ids", {2}}})
                 .contains("error"));
    REQUIRE(!fixture
                 .call("add_modulation",
                       {{"profile_id", 1},
                        {"source_type", static_cast<int>(ModulationSourceType::AudioFollower)},
                        {"source_sidechain_part", 2},
                        {"target", "source.filter.cutoff"},
                        {"depth", 0.5}})
                 .contains("error"));
    const auto referenced = fixture.state();
    CHECK(fixture.call("score_remove_part", {{"score_id", 1}, {"part_id", 2}}).contains("error"));
    CHECK(fixture.state() == referenced);
    REQUIRE(!fixture.call("remove_timbre_modulation", {{"profile_id", 1}, {"index", 0}})
                 .contains("error"));
    REQUIRE(
        !fixture.call("score_remove_part", {{"score_id", 1}, {"part_id", 2}}).contains("error"));
    CHECK(fixture.session.timbre->find(2) == nullptr);
    CHECK(fixture.session.mix->find(1)->group_buses[0].member_channels.empty());
    REQUIRE(!fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    CHECK(std::get<FMSynth>(fixture.session.timbre->find(2)->source.data).feedback ==
          Catch::Approx(0.25));
    CHECK(fixture.session.mix->find(1)->channels[1].fader.level_db == Catch::Approx(-12));
    CHECK(fixture.session.mix->find(1)->group_buses[0].member_channels ==
          std::vector<ChannelStripId>{ChannelStripId{2}});
    REQUIRE(!fixture.call("score_reorder_parts", {{"score_id", 1}, {"part_ids", {2, 1}}})
                 .contains("error"));
    CHECK(
        (fixture.session.project->projects.at(1).profile_ids == std::vector<std::uint64_t>{2, 1}));
    CHECK(fixture.session.score->find(1)->parts[0].id == PartId{2});
    CHECK(fixture.session.mix->find(1)->channels[0].id == ChannelStripId{2});
    CHECK(fixture.session.mix->find(1)->channels[0].part_id == PartId{2});
}

TEST_CASE("Shared preset history retains other projects and refuses incoming morph references",
          "[mcp][project][authoring]") {
    ProjectFixture fixture;
    const auto first =
        fixture.call("save_preset", {{"profile_id", 1}, {"name", "First"}})["preset_id"];
    const auto second_score = fixture.create_score(1);
    const auto second_project = fixture.call("create_project", {{"score_id", second_score}});
    const auto second_profile = second_project["timbre_profile_ids"][0];
    const auto second = fixture.call(
        "save_preset", {{"profile_id", second_profile}, {"name", "Second"}})["preset_id"];
    REQUIRE(!fixture
                 .call("morph_presets",
                       {{"profile_id", second_profile},
                        {"from_preset_id", first},
                        {"to_preset_id", first},
                        {"start_bar", 1},
                        {"end_bar", 2}})
                 .contains("error"));
    const auto referenced = fixture.state();
    CHECK(fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.state() == referenced);
    REQUIRE(!fixture.call("score_undo", {{"score_id", second_score}}).contains("error"));
    REQUIRE(!fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    REQUIRE(fixture.session.timbre->preset_library.size() == 1);
    CHECK(fixture.session.timbre->preset_library[0].id.value == second.get<std::uint64_t>());
    CHECK(fixture.session.timbre->next_preset_id == 3);
    REQUIRE(!fixture.call("score_redo", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.session.timbre->preset_library.size() == 2);
}

TEST_CASE("Shared preset undo admits a retained morph satisfied by its profile's local preset",
          "[mcp][project][history][preset]") {
    ProjectFixture fixture;
    const auto saved = fixture.call("save_preset", {{"profile_id", 1}, {"name", "Global"}});
    REQUIRE_FALSE(saved.contains("error"));
    auto retained = create_timbre_profile(TimbreProfileId{2}, PartId{1}, "Scratch local owner");
    SubtractiveSynth source;
    source.oscillators.push_back(Oscillator{});
    retained.source.data = std::move(source);
    REQUIRE(set_parameter(retained, "source.filter.cutoff", 1200));
    retained.presets.push_back(save_preset(
        retained, fixture.session.timbre->preset_library[0].id, "Distinct local preset"));
    REQUIRE(timbre_preset_to_json(retained.presets[0]) !=
            timbre_preset_to_json(fixture.session.timbre->preset_library[0]));
    const auto id = retained.presets[0].id;
    retained.preset_morphs.push_back(PresetMorph{
        id, id, ScoreTime{1, Beat::zero()}, ScoreTime{2, Beat::zero()}, MappingCurve{}});
    fixture.session.timbre->profiles.emplace(2, std::move(retained));
    fixture.session.timbre->next_profile_id = 3;
    const auto before = timbre_to_json(*fixture.session.timbre->find(2));
    REQUIRE_FALSE(fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.session.timbre->preset_library.empty());
    CHECK(timbre_to_json(*fixture.session.timbre->find(2)) == before);
    REQUIRE_FALSE(fixture.call("score_redo", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.session.timbre->preset_library.size() == 1);
    CHECK(timbre_to_json(*fixture.session.timbre->find(2)) == before);
}

TEST_CASE("Bound identity retirement preserves imported holes without resurrecting exposed IDs",
          "[mcp][project][authoring]") {
    ProjectFixture fixture;
    REQUIRE(
        !fixture.call("score_add_part", {{"score_id", 1}, {"name", "Retire"}}).contains("error"));
    REQUIRE(!fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    const auto next = fixture.call("score_add_part", {{"score_id", 1}, {"name", "Fresh"}});
    CHECK(next["part_id"] == 3);
    CHECK(next["channel_id"] == 3);

    // An imported high ID reserves that identity, not every smaller integer.
    ProjectFixture imported;
    const auto high = std::numeric_limits<std::uint64_t>::max();
    imported.session.score->find(1)->parts[0].id = PartId{high};
    imported.session.score->find(1)->identity_reservations = {};
    imported.session.timbre->find(1)->part_id = PartId{high};
    imported.session.mix->find(1)->channels[0].part_id = PartId{high};
    imported.session.mix->find(1)->channels[0].id = ChannelStripId{high};
    imported.session.project->projects.at(1).reserved_channel_ids = {high};
    const auto hole = imported.call("score_add_part", {{"score_id", 1}, {"name", "Hole"}});
    REQUIRE(!hole.contains("error"));
    CHECK(hole["part_id"] == 1);
    CHECK(hole["channel_id"] == 1);
}

TEST_CASE("Project undo retains typed Score event reservations before a new note edit",
          "[mcp][project][authoring]") {
    ProjectFixture fixture;
    const auto insert = [&](int letter) {
        return fixture.call(
            "score_insert_note",
            {{"score_id", 1},
             {"part_id", 1},
             {"bar", 1},
             {"pitch", {{"letter", letter == 0 ? "C" : "D"}, {"accidental", 0}, {"octave", 4}}},
             {"duration", {{"n", 1}, {"d", 4}}}});
    };
    const auto note_id = [&] {
        const auto& events = fixture.session.score->find(1)->parts[0].measures[0].voices[0].events;
        const auto note = std::ranges::find_if(events, [](const auto& event) {
            return std::holds_alternative<NoteGroup>(event.payload);
        });
        REQUIRE(note != events.end());
        return note->id;
    };
    REQUIRE(!insert(0).contains("error"));
    const auto retired = note_id();
    REQUIRE(!fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    REQUIRE(!insert(1).contains("error"));
    CHECK(note_id() != retired);
    CHECK(fixture.session.score->find(1)->identity_reservations.events.contains(retired));
}

TEST_CASE("Project history capacity bounds the complete shared authoring timeline",
          "[mcp][project][authoring]") {
    ProjectFixture fixture;
    fixture.session.project->projects.at(1).history_capacity = 2;
    for (const auto value : {2000, 3000, 4000})
        REQUIRE(!fixture
                     .call("set_parameter",
                           {{"profile_id", 1}, {"path", "source.filter.cutoff"}, {"value", value}})
                     .contains("error"));
    CHECK(fixture.session.project->projects.at(1).undo_entries.size() == 2);
    REQUIRE(!fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    REQUIRE(!fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    CHECK(*get_parameter(*fixture.session.timbre->find(1), "source.filter.cutoff") ==
          Catch::Approx(2000));
    CHECK(fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
}

TEST_CASE("Project history preserves finite quiet Mix levels and rejects infinity atomically",
          "[mcp][project][authoring]") {
    ProjectFixture fixture;
    const auto before = fixture.state();
    CHECK(fixture
              .call("set_channel_level",
                    {{"graph_id", 1},
                     {"channel_id", 1},
                     {"level_db", -std::numeric_limits<double>::infinity()}})
              .contains("error"));
    CHECK(fixture.state() == before);
    const auto silence = -120.0;
    REQUIRE(
        !fixture
             .call("set_channel_level", {{"graph_id", 1}, {"channel_id", 1}, {"level_db", silence}})
             .contains("error"));
    CHECK(fixture.session.mix->find(1)->channels[0].fader.level_db == Catch::Approx(-120));
    REQUIRE(!fixture
                 .call("set_parameter",
                       {{"profile_id", 1}, {"path", "source.filter.cutoff"}, {"value", 3000}})
                 .contains("error"));
    REQUIRE(!fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.session.mix->find(1)->channels[0].fader.level_db == Catch::Approx(-120));
    REQUIRE(!fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.session.mix->find(1)->channels[0].fader.level_db == Catch::Approx(0));
    REQUIRE(!fixture.call("score_redo", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.session.mix->find(1)->channels[0].fader.level_db == Catch::Approx(-120));
}

TEST_CASE("Owning input trim edits retain the fader and reject out-of-domain values atomically",
          "[mcp][project][authoring][input-trim]") {
    ProjectFixture fixture;
    const auto before = fixture.state();
    for (const auto value : {-24.0000001, 24.0000001, std::numeric_limits<double>::infinity()}) {
        CHECK(fixture
                  .call("set_channel_input_trim",
                        {{"graph_id", 1}, {"channel_id", 1}, {"input_trim_db", value}})
                  .contains("error"));
        CHECK(fixture.state() == before);
    }
    const auto original_fader = fixture.session.mix->find(1)->channels[0].fader.level_db;
    REQUIRE(!fixture
                 .call("set_channel_input_trim",
                       {{"graph_id", 1}, {"channel_id", 1}, {"input_trim_db", -7.5}})
                 .contains("error"));
    CHECK(fixture.session.mix->find(1)->channels[0].input_trim == -7.5f);
    CHECK(fixture.session.mix->find(1)->channels[0].fader.level_db == original_fader);
    REQUIRE(!fixture.call("score_undo", {{"score_id", 1}}).contains("error"));
    CHECK(fixture.session.mix->find(1)->channels[0].input_trim == 0.0f);
    CHECK(fixture.session.mix->find(1)->channels[0].fader.level_db == original_fader);
}
