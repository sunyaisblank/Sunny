/** Workspace publication and real filesystem recovery through public APIs. */
#include <atomic>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <sunny/core/corpus/serialization.hpp>
#include <sunny/core/corpus/workflows.hpp>
#include <sunny/core/mix/workflows.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/score/workflows.hpp>
#include <sunny/core/timbre/workflows.hpp>
#include <sunny/infrastructure/mcp/mix_tools.hpp>
#include <sunny/infrastructure/mcp/project_tools.hpp>
#include <sunny/infrastructure/mcp/score_tools.hpp>
#include <sunny/infrastructure/mcp/timbre_tools.hpp>
#include <sunny/infrastructure/mcp/workspace_state.hpp>

using namespace sunny::core;
using namespace sunny::infrastructure;
using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

struct Directory {
    fs::path path;
    Directory() {
        static std::atomic<unsigned> nonce{0};
        path = fs::temp_directory_path() /
               ("sunny-workspace-test-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                std::to_string(nonce.fetch_add(1)));
        REQUIRE(fs::create_directory(path));
    }
    ~Directory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

void write_bytes(const fs::path& path, const std::string& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    REQUIRE(file.good());
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.close();
    REQUIRE(file.good());
}

std::string bytes(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.good());
    return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

json snapshot(const McpSession& session) {
    auto encoded = workspace_to_json(session);
    INFO((encoded ? "valid workspace" : encoded.error().message));
    REQUIRE(encoded.has_value());
    return std::move(*encoded);
}

void no_temporaries(const Directory& directory) {
    for (const auto& entry : fs::directory_iterator(directory.path))
        CHECK(entry.path().filename().string().find(".sunny-tmp-") == std::string::npos);
}

struct Fixture {
    McpSession session;
    explicit Fixture(std::uint64_t id = 1) {
        ScoreSpec spec;
        spec.id = ScoreId{id};
        spec.title = "Saved project " + std::to_string(id);
        spec.total_bars = 2;
        spec.bpm = 120;
        spec.key_root = SpelledPitch{0, 0, 4};
        spec.parts.resize(1);
        spec.parts[0].name = "Synth";
        spec.parts[0].instrument_type = InstrumentType::Synthesiser;
        auto score = create_score(spec);
        REQUIRE(score.has_value());
        score->identity_reservations.parts.insert(PartId{9});
        score->identity_reservations.events.insert(EventId{10});
        score->identity_reservations.sections.insert(SectionId{11});
        score->identity_reservations.tuplets.insert(TupletId{12});
        score->identity_reservations.beams.insert(BeamGroupId{13});
        session.score->scores.emplace(id, std::move(*score));
        session.score->next_score_id = id + 1;
        auto profile = create_timbre_profile(TimbreProfileId{id}, PartId{1}, "Profile");
        REQUIRE(set_parameter(profile, "source.filter.cutoff", 3800));
        REQUIRE(add_effect(profile, Effect{EffectId{1}, DistortionEffect{}, true, 0.25f}));
        auto preset = save_preset(profile, TimbrePresetId{id}, "Shared preset");
        session.timbre->preset_library.push_back(std::move(preset));
        REQUIRE(morph_presets(profile,
                              PresetMorph{TimbrePresetId{id},
                                          TimbrePresetId{id},
                                          ScoreTime{1, Beat{0, 1}},
                                          ScoreTime{2, Beat{0, 1}},
                                          MappingCurve{}},
                              session.timbre->preset_library));
        session.timbre->profiles.emplace(id, std::move(profile));
        session.timbre->next_profile_id = id + 1;
        session.timbre->next_effect_id = 2;
        session.timbre->next_preset_id = id + 1;
        auto mix = create_mix_graph(MixGraphId{id}, {PartId{1}});
        REQUIRE(set_channel_level(mix, ChannelStripId{1}, -8));
        REQUIRE(create_group_bus(mix, GroupBusId{1}, "Bus", {ChannelStripId{1}}));
        REQUIRE(add_channel_effect(
            mix, ChannelStripId{1}, MixEffect{MixEffectId{1}, MixCompressor{}, true, {}}));
        session.mix->graphs.emplace(id, std::move(mix));
        session.mix->next_graph_id = id + 1;
        session.mix->next_group_id = 2;
        session.mix->next_effect_id = 2;
        auto composer = create_composer_profile(ComposerProfileId{id}, "Composer");
        session.corpus->corpus.composers.emplace(id, std::move(composer));
        WorkMetadata metadata;
        metadata.title = "Unanalysed evidence";
        metadata.composer = ComposerProfileId{0};
        metadata.source_format = "midi";
        auto work = create_ingested_work(IngestedWorkId{id}, metadata);
        work.score = session.score->scores.at(id);
        session.corpus->corpus.works.emplace(id, std::move(work));
        REQUIRE(assign_work_to_composer(
            session.corpus->corpus, IngestedWorkId{id}, ComposerProfileId{id}));
        session.corpus->next_composer_id = id + 1;
        session.corpus->next_work_id = id + 1;
        ProjectRecord project;
        project.score_id = id;
        project.mix_graph_id = id;
        project.profile_ids = {id};
        project.reserved_channel_ids = {1, 7};
        session.project->projects.emplace(id, std::move(project));
        session.deployment->next_plan_id = 18;
    }
};

json call(McpServer& server, const std::string& name, const json& arguments) {
    const auto response =
        server.process_request({{"jsonrpc", "2.0"},
                                {"id", 1},
                                {"method", "tools/call"},
                                {"params", {{"name", name}, {"arguments", arguments}}}});
    INFO(response.dump());
    REQUIRE(response.contains("result"));
    return json::parse(response["result"]["content"][0]["text"].get<std::string>());
}

} // namespace

TEST_CASE(
    "Workspace canonical text restores every authored store and reservation in fresh shared slots",
    "[mcp][workspace][persistence]") {
    Fixture fixture;
    Directory directory;
    const auto path = directory.path / "workspace.json";
    const auto original = snapshot(fixture.session);
    CHECK(original["scores"][0]["document"]["schema_version"] == SCORE_IR_SCHEMA_VERSION);
    CHECK(original["corpus"]["schema_version"] == CORPUS_IR_SCHEMA_VERSION);
    CHECK_FALSE(original.contains("deployment_plans"));
    CHECK_FALSE(original["projects"][0].contains("undo_entries"));
    const auto saved = save_workspace(fixture.session, path);
    REQUIRE(saved.committed);
#ifndef _WIN32
    REQUIRE(saved.success);
    CHECK(saved.durability_confirmed);
#endif
    McpSession restarted;
    const auto* score_store = restarted.score.get();
    const auto* timbre_store = restarted.timbre.get();
    const auto* mix_store = restarted.mix.get();
    auto opened = open_workspace(restarted, path);
    INFO((opened ? "opened" : opened.error().message));
    REQUIRE(opened.has_value());
    CHECK(restarted.score.get() == score_store);
    CHECK(restarted.timbre.get() == timbre_store);
    CHECK(restarted.mix.get() == mix_store);
    CHECK(snapshot(restarted) == original);
    CHECK(restarted.corpus->corpus.works.at(1).score.has_value());
    CHECK_FALSE(restarted.corpus->corpus.works.at(1).analysis_complete);
    CHECK(restarted.score->find(1)->identity_reservations.events.contains(EventId{10}));
    CHECK(restarted.score->find(1)->identity_reservations.beams.contains(BeamGroupId{13}));
    CHECK(restarted.project->projects.at(1).reserved_channel_ids.contains(7));
    CHECK(restarted.project->projects.at(1).undo_entries.empty());
    CHECK(restarted.deployment->plans.empty());
    CHECK(restarted.deployment->next_plan_id == 18);
}

TEST_CASE("Corrupt workspace candidates cannot change live documents history plans or counters",
          "[mcp][workspace][persistence]") {
    Fixture fixture;
    Directory directory;
    const auto path = directory.path / "candidate.json";
    const auto valid = snapshot(fixture.session);
    fixture.session.project->projects.at(1).undo_entries.push_back(ProjectHistoryEntry{});
    fixture.session.deployment->plans.emplace(17, StoredProjectDeploymentPlan{});
    std::vector<json> corrupt;
    auto bad = valid;
    bad["scores"][0]["id"] = 2;
    corrupt.push_back(bad);
    bad = valid;
    bad["scores"].push_back(bad["scores"][0]);
    corrupt.push_back(bad);
    bad = valid;
    bad["mix_graphs"][0]["document"]["id"] = 2;
    corrupt.push_back(bad);
    bad = valid;
    bad["projects"][0]["timbre_profile_ids"] = {999};
    corrupt.push_back(bad);
    bad = valid;
    bad["projects"].push_back(bad["projects"][0]);
    corrupt.push_back(bad);
    bad = valid;
    bad["projects"][0]["reserved_channel_ids"] = {7};
    corrupt.push_back(bad);
    bad = valid;
    bad["projects"][0]["reserved_channel_ids"] = {7, 1};
    corrupt.push_back(bad);
    bad = valid;
    bad["counters"]["next_profile_id"] = 1;
    corrupt.push_back(bad);
    bad = valid;
    bad["scores"][0]["document"]["schema_version"] = 999;
    corrupt.push_back(bad);
    bad = valid;
    bad["version"] = WORKSPACE_SCHEMA_VERSION + 1;
    corrupt.push_back(bad);
    for (const auto& candidate : corrupt) {
        INFO(candidate.dump());
        write_bytes(path, candidate.dump());
        CHECK_FALSE(open_workspace(fixture.session, path).has_value());
        CHECK(snapshot(fixture.session) == valid);
        CHECK(fixture.session.project->projects.at(1).undo_entries.size() == 1);
        CHECK(fixture.session.deployment->plans.contains(17));
        CHECK(fixture.session.deployment->next_plan_id == 18);
    }
    write_bytes(path, "{\"format\":\"sunny-workspace\",\"format\":\"sunny-workspace\"}");
    const auto duplicate = open_workspace(fixture.session, path);
    REQUIRE_FALSE(duplicate.has_value());
    CHECK(duplicate.error().message.find("duplicate") != std::string::npos);
    CHECK(snapshot(fixture.session) == valid);
}

TEST_CASE(
    "Workspace envelope delegates supported child migrations without guessing typed reservations",
    "[mcp][workspace][persistence]") {
    Fixture fixture;
    auto legacy = snapshot(fixture.session);
    auto& score = legacy["scores"][0]["document"];
    score["schema_version"] = 8;
    score.erase("identity_reservations");
    auto loaded = workspace_from_json(legacy);
    INFO((loaded ? "migrated" : loaded.error().message));
    REQUIRE(loaded.has_value());
    CHECK(loaded->score.scores.at(1).identity_reservations.parts.contains(PartId{1}));
    // The child cannot encode retirement, but the explicit outer namespace history can.
    CHECK(loaded->score.scores.at(1).identity_reservations.parts.contains(PartId{9}));
    CHECK(loaded->project.projects.at(1).reserved_channel_ids.contains(7));
}

TEST_CASE("Persisted local UINT64_MAX reservations preserve unused holes while store counter "
          "exhaustion is precise",
          "[mcp][workspace][persistence]") {
    Fixture fixture;
    const auto high = std::numeric_limits<std::uint64_t>::max();
    fixture.session.score->find(1)->parts[0].id = PartId{high};
    fixture.session.score->find(1)->identity_reservations.parts.clear();
    fixture.session.timbre->find(1)->part_id = PartId{high};
    fixture.session.mix->find(1)->channels[0].part_id = PartId{high};
    fixture.session.mix->find(1)->channels[0].id = ChannelStripId{high};
    fixture.session.mix->find(1)->group_buses[0].member_channels = {ChannelStripId{high}};
    fixture.session.project->projects.at(1).reserved_channel_ids = {high};
    auto encoded = snapshot(fixture.session);
    Directory directory;
    const auto path = directory.path / "high.json";
    write_bytes(path, encoded.dump());
    McpSession restored;
    McpServer server;
    register_score_tools(server, restored.score);
    register_timbre_tools(server, restored.timbre);
    register_mix_tools(server, restored.mix);
    register_project_tools(server, restored);
    REQUIRE(!call(server, "workspace_open", {{"path", path.string()}}).contains("error"));
    const auto added = call(server, "score_add_part", {{"score_id", 1}, {"name", "Unused hole"}});
    REQUIRE(!added.contains("error"));
    CHECK(added["part_id"] == 1);
    CHECK(added["channel_id"] == 1);

    encoded["scores"][0]["id"] = high;
    encoded["scores"][0]["document"]["id"] = high;
    encoded["projects"][0]["score_id"] = high;
    encoded["namespace_history"]["scores"][0]["score_id"] = high;
    encoded["namespace_history"]["projects"][0]["score_id"] = high;
    encoded["counters"]["next_score_id"] = high;
    const auto exhausted = workspace_from_json(encoded);
    REQUIRE_FALSE(exhausted.has_value());
    CHECK(exhausted.error().message.find("exhausted") != std::string::npos);
}

TEST_CASE(
    "Workspace import rejects store and preset collisions while preserving local identity scopes",
    "[mcp][workspace][persistence]") {
    Fixture first;
    Fixture second(2);
    Directory directory;
    const auto path = directory.path / "incoming.json";
    write_bytes(path, snapshot(second.session).dump());
    first.session.project->projects.at(1).undo_entries.push_back(ProjectHistoryEntry{});
    first.session.deployment->plans.emplace(17, StoredProjectDeploymentPlan{});
    first.session.deployment->next_plan_id = 31;
    auto imported = import_workspace(first.session, path);
    INFO((imported ? "imported" : imported.error().message));
    REQUIRE(imported.has_value());
    CHECK(first.session.score->scores.size() == 2);
    CHECK(first.session.score->find(1)->parts[0].id == first.session.score->find(2)->parts[0].id);
    CHECK(first.session.timbre->find(1)->insert_chain.effects[0].id ==
          first.session.timbre->find(2)->insert_chain.effects[0].id);
    CHECK(first.session.mix->find(1)->group_buses[0].id ==
          first.session.mix->find(2)->group_buses[0].id);
    CHECK(first.session.project->projects.at(1).undo_entries.empty());
    CHECK(first.session.deployment->plans.empty());
    CHECK(first.session.deployment->next_plan_id == 31);
    const auto merged = snapshot(first.session);
    CHECK_FALSE(import_workspace(first.session, path).has_value());
    CHECK(snapshot(first.session) == merged);

    Fixture preset_collision(3);
    preset_collision.session.timbre->preset_library[0].id = TimbrePresetId{1};
    auto& morph = preset_collision.session.timbre->find(3)->preset_morphs[0];
    morph.from_preset = morph.to_preset = TimbrePresetId{1};
    write_bytes(path, snapshot(preset_collision.session).dump());
    const auto refused = import_workspace(first.session, path);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().message.find("preset_library") != std::string::npos);
    CHECK(snapshot(first.session) == merged);
}

TEST_CASE("Same-ID workspace replacement advances publication versions and retains retired IDs",
          "[mcp][workspace][persistence][version]") {
    Fixture fixture;
    Directory directory;
    const auto path = directory.path / "older.json";
    REQUIRE(save_workspace(fixture.session, path).committed);
    McpServer server;
    register_score_tools(server, fixture.session.score);
    register_timbre_tools(server, fixture.session.timbre);
    register_mix_tools(server, fixture.session.mix);
    register_project_tools(server, fixture.session);
    const auto added = call(server, "score_add_part", {{"score_id", 1}, {"name", "Later"}});
    REQUIRE_FALSE(added.contains("error"));
    const auto retired_part = PartId{added["part_id"].get<std::uint64_t>()};
    const auto retired_channel = added["channel_id"].get<std::uint64_t>();
    const auto version = fixture.session.score->find(1)->version;
    const auto revision = fixture.session.project->projects.at(1).revision;
    REQUIRE(open_workspace(fixture.session, path));
    CHECK(fixture.session.score->find(1)->parts.size() == 1);
    CHECK(fixture.session.score->find(1)->version > version);
    CHECK(fixture.session.project->projects.at(1).revision > revision);
    CHECK(fixture.session.score->find(1)->identity_reservations.parts.contains(retired_part));
    CHECK(fixture.session.project->projects.at(1).reserved_channel_ids.contains(retired_channel));
    CHECK(fixture.session.project->projects.at(1).undo_entries.empty());
    fixture.session.score->find(1)->version = std::numeric_limits<std::uint64_t>::max();
    const auto before = snapshot(fixture.session);
    CHECK_FALSE(open_workspace(fixture.session, path));
    CHECK(snapshot(fixture.session) == before);
}

TEST_CASE("An absent workspace namespace retains exposed identities and publication floors",
          "[mcp][workspace][identity][namespace-history]") {
    Fixture fixture;
    Directory directory;
    const auto older = directory.path / "older-A.json";
    const auto empty = directory.path / "empty-B.json";
    REQUIRE(save_workspace(fixture.session, older).committed);
    McpSession blank;
    REQUIRE(save_workspace(blank, empty).committed);
    McpServer server;
    register_score_tools(server, fixture.session.score);
    register_timbre_tools(server, fixture.session.timbre);
    register_mix_tools(server, fixture.session.mix);
    register_project_tools(server, fixture.session);
    const auto first = call(server, "score_add_part", {{"score_id", 1}, {"name", "Exposed"}});
    REQUIRE_FALSE(first.contains("error"));
    REQUIRE(first["part_id"] == 2);
    REQUIRE(first["channel_id"] == 2);
    const auto version = fixture.session.score->find(1)->version;
    const auto revision = fixture.session.project->projects.at(1).revision;
    REQUIRE(open_workspace(fixture.session, empty));
    CHECK(fixture.session.score->scores.empty());
    CHECK(fixture.session.mix->graphs.empty());
    CHECK(fixture.session.project->projects.empty());
    const auto& inactive = fixture.session.namespace_history->scores.at(1);
    CHECK(inactive.version_floor == version);
    CHECK(inactive.identities.parts.contains(PartId{2}));
    CHECK(inactive.identities.events.contains(EventId{3}));
    CHECK(inactive.identities.events.contains(EventId{4}));
    CHECK(fixture.session.namespace_history->graph_channels.at(1).contains(2));
    CHECK(fixture.session.namespace_history->project_revision_floors.at(1) == revision);
    REQUIRE(open_workspace(fixture.session, older));
    CHECK(fixture.session.score->find(1)->version > version);
    CHECK(fixture.session.project->projects.at(1).revision > revision);
    CHECK(fixture.session.score->find(1)->identity_reservations.parts.contains(PartId{2}));
    CHECK(fixture.session.project->projects.at(1).reserved_channel_ids.contains(2));
    const auto second = call(server, "score_add_part", {{"score_id", 1}, {"name", "Different"}});
    REQUIRE_FALSE(second.contains("error"));
    CHECK(second["part_id"] == 3);
    CHECK(second["channel_id"] == 3);
    CHECK(second["timbre_profile_id"] != first["timbre_profile_id"]);
}

TEST_CASE("Saving an empty active workspace persists inactive retirement through restart",
          "[mcp][workspace][identity][namespace-history][restart]") {
    Fixture fixture;
    Directory directory;
    const auto older = directory.path / "older-A.json";
    const auto empty = directory.path / "empty-B.json";
    const auto retired = directory.path / "saved-B.json";
    REQUIRE(save_workspace(fixture.session, older).committed);
    McpSession blank;
    REQUIRE(save_workspace(blank, empty).committed);
    McpServer server;
    register_score_tools(server, fixture.session.score);
    register_timbre_tools(server, fixture.session.timbre);
    register_mix_tools(server, fixture.session.mix);
    register_project_tools(server, fixture.session);
    const auto first = call(server, "score_add_part", {{"score_id", 1}, {"name", "Exposed"}});
    REQUIRE(first.at("part_id") == 2);
    const auto version = fixture.session.score->find(1)->version;
    const auto revision = fixture.session.project->projects.at(1).revision;
    REQUIRE(open_workspace(fixture.session, empty));
    REQUIRE(save_workspace(fixture.session, retired).committed);
    const auto encoded = json::parse(bytes(retired));
    CHECK(encoded["scores"].empty());
    CHECK(encoded["projects"].empty());
    CHECK(encoded["namespace_history"]["scores"][0]["version_floor"] == version);
    CHECK(encoded["namespace_history"]["projects"][0]["revision_floor"] == revision);
    McpSession restarted;
    REQUIRE(open_workspace(restarted, retired));
    CHECK(restarted.score->scores.empty());
    CHECK(restarted.namespace_history->scores.at(1).identities.parts.contains(PartId{2}));
    REQUIRE(open_workspace(restarted, older));
    CHECK(restarted.score->find(1)->version > version);
    CHECK(restarted.project->projects.at(1).revision > revision);
    McpServer fresh_server;
    register_score_tools(fresh_server, restarted.score);
    register_timbre_tools(fresh_server, restarted.timbre);
    register_mix_tools(fresh_server, restarted.mix);
    register_project_tools(fresh_server, restarted);
    const auto next = call(fresh_server, "score_add_part", {{"score_id", 1}, {"name", "New"}});
    REQUIRE_FALSE(next.contains("error"));
    CHECK(next["part_id"] == 3);
    CHECK(next["channel_id"] == 3);
}

TEST_CASE("Namespace history rejects malformed records and insufficient global counters atomically",
          "[mcp][workspace][namespace-history][admission]") {
    Fixture fixture;
    Directory directory;
    const auto path = directory.path / "invalid-history.json";
    const auto before = snapshot(fixture.session);
    std::vector<json> invalid;
    auto candidate = before;
    candidate.erase("namespace_history");
    invalid.push_back(candidate);
    for (const auto* key : {"scores", "mix_graphs", "projects"}) {
        candidate = before;
        candidate["namespace_history"][key].push_back(candidate["namespace_history"][key][0]);
        invalid.push_back(candidate);
    }
    for (const auto& malformed :
         {json::array({1, 1}), json::array({9, 1}), json::array({0}), json::array({1.5})}) {
        candidate = before;
        candidate["namespace_history"]["scores"][0]["identities"]["parts"] = malformed;
        invalid.push_back(candidate);
    }
    candidate = before;
    candidate["namespace_history"]["scores"][0]["version_floor"] = 0;
    invalid.push_back(candidate);
    candidate = before;
    candidate["namespace_history"]["scores"][0]["version_floor"] = 71;
    invalid.push_back(candidate);
    candidate = before;
    candidate["namespace_history"]["scores"][0]["identities"]["events"] = json::array();
    invalid.push_back(candidate);
    candidate = before;
    candidate["namespace_history"]["mix_graphs"][0]["channel_ids"] = json::array({1, 1});
    invalid.push_back(candidate);
    candidate = before;
    candidate["namespace_history"]["mix_graphs"][0]["channel_ids"] = json::array({1});
    invalid.push_back(candidate);
    for (const auto* key : {"scores", "mix_graphs", "projects"}) {
        candidate = before;
        auto record = candidate["namespace_history"][key][0];
        record[key == std::string_view("mix_graphs") ? "mix_graph_id" : "score_id"] = 73;
        candidate["namespace_history"][key].push_back(std::move(record));
        invalid.push_back(candidate); // Retired namespace keys count against global counters.
    }
    for (const auto& document : invalid) {
        write_bytes(path, document.dump());
        CHECK_FALSE(open_workspace(fixture.session, path));
        CHECK(snapshot(fixture.session) == before);
    }
}

TEST_CASE("Inactive MAX publication floors prevent older activation without partial publication",
          "[mcp][workspace][namespace-history][exhaustion]") {
    for (const bool project_floor : {false, true}) {
        Fixture fixture;
        Directory directory;
        const auto older = directory.path / "older.json";
        const auto empty = directory.path / "empty.json";
        const auto retired = directory.path / "retired.json";
        REQUIRE(save_workspace(fixture.session, older).committed);
        McpSession blank;
        REQUIRE(save_workspace(blank, empty).committed);
        if (project_floor)
            fixture.session.project->projects.at(1).revision =
                std::numeric_limits<std::uint64_t>::max();
        else
            fixture.session.score->find(1)->version = std::numeric_limits<std::uint64_t>::max();
        REQUIRE(open_workspace(fixture.session, empty));
        REQUIRE(save_workspace(fixture.session, retired).committed);
        McpSession restarted;
        REQUIRE(open_workspace(restarted, retired));
        const auto before = snapshot(restarted);
        CHECK_FALSE(open_workspace(restarted, older));
        CHECK(snapshot(restarted) == before);
        CHECK(restarted.score->scores.empty());
        CHECK(restarted.project->projects.empty());
    }
}

TEST_CASE("Import merges inactive history without revising unrelated unchanged namespaces",
          "[mcp][workspace][namespace-history][import]") {
    Fixture current{2};
    Fixture incoming{1};
    Directory directory;
    const auto path = directory.path / "incoming.json";
    auto document = snapshot(incoming.session);
    REQUIRE(save_workspace(incoming.session, path).committed);
    const auto version = current.session.score->find(2)->version;
    const auto revision = current.session.project->projects.at(2).revision;
    REQUIRE(import_workspace(current.session, path));
    CHECK(current.session.score->find(2)->version == version);
    CHECK(current.session.project->projects.at(2).revision == revision);

    Fixture collided{2};
    auto inactive_score = document["namespace_history"]["scores"][0];
    inactive_score["score_id"] = 2;
    inactive_score["version_floor"] = 100;
    inactive_score["identities"]["parts"].push_back(99);
    document["namespace_history"]["scores"].push_back(inactive_score);
    document["namespace_history"]["projects"].push_back({{"score_id", 2}, {"revision_floor", 200}});
    document["counters"]["next_score_id"] = 3;
    write_bytes(path, document.dump());
    REQUIRE(import_workspace(collided.session, path));
    CHECK(collided.session.score->find(2)->version == 101);
    CHECK(collided.session.project->projects.at(2).revision == 201);
    CHECK(collided.session.score->find(2)->identity_reservations.parts.contains(PartId{99}));
    CHECK(collided.session.score->find(2)->parts.size() == 1);
}

TEST_CASE("Inactive import retirement advances every changed publication and its owning project",
          "[mcp][workspace][namespace-history][import][version]") {
    for (const auto* change : {"equal IDs", "lower IDs", "higher Score floor", "Channels"}) {
        INFO(change);
        Fixture current{2};
        Fixture unrelated{3};
        Directory directory;
        const auto other = directory.path / "unrelated.json";
        REQUIRE(save_workspace(unrelated.session, other).committed);
        REQUIRE(import_workspace(current.session, other));
        current.session.score->find(2)->version = 7;
        current.session.project->projects.at(2).revision = 11;
        const auto before = snapshot(current.session);
        McpSession empty;
        auto incoming = snapshot(empty);
        const bool channels = change == std::string_view("Channels");
        const bool higher = change == std::string_view("higher Score floor");
        if (channels) {
            incoming["namespace_history"]["mix_graphs"].push_back(
                {{"mix_graph_id", 2}, {"channel_ids", json::array({99})}});
            incoming["counters"]["next_graph_id"] = 3;
        } else {
            auto history = before["namespace_history"]["scores"][0];
            history["version_floor"] = higher                                    ? 20
                                       : change == std::string_view("lower IDs") ? 6
                                                                                 : 7;
            if (!higher) history["identities"]["parts"].push_back(99);
            incoming["namespace_history"]["scores"].push_back(std::move(history));
            incoming["counters"]["next_score_id"] = 3;
        }
        const auto path = directory.path / "retirement.json";
        write_bytes(path, incoming.dump());
        const auto imported = import_workspace(current.session, path);
        INFO((imported ? "imported" : imported.error().message));
        REQUIRE(imported);
        CHECK(current.session.score->find(2)->version == (channels ? 7 : higher ? 21 : 8));
        CHECK(current.session.project->projects.at(2).revision == 12);
        if (channels)
            CHECK(current.session.project->projects.at(2).reserved_channel_ids.contains(99));
        else if (!higher)
            CHECK(current.session.score->find(2)->identity_reservations.parts.contains(PartId{99}));
        const auto after = snapshot(current.session);
        CHECK(after["scores"][1] == before["scores"][1]);
        CHECK(after["projects"][1] == before["projects"][1]);
        CHECK(after["mix_graphs"][1] == before["mix_graphs"][1]);
        CHECK(after["timbre_profiles"][1] == before["timbre_profiles"][1]);
    }
}

TEST_CASE("Import retirement refuses exhausted Score and project publications atomically",
          "[mcp][workspace][namespace-history][import][exhaustion]") {
    for (const auto* change : {"equal IDs", "higher Score floor", "Channels"}) {
        for (const bool score_exhausted : {false, true}) {
            INFO(change << " Score exhausted=" << score_exhausted);
            Fixture current{2};
            const bool channels = change == std::string_view("Channels");
            const bool higher = change == std::string_view("higher Score floor");
            // Channel-only retirement does not change Score, so exercise its owning revision.
            if (channels && score_exhausted) continue;
            if (score_exhausted && !higher)
                current.session.score->find(2)->version = std::numeric_limits<std::uint64_t>::max();
            if (!score_exhausted)
                current.session.project->projects.at(2).revision =
                    std::numeric_limits<std::uint64_t>::max();
            const auto before = snapshot(current.session);
            McpSession empty;
            auto incoming = snapshot(empty);
            if (channels) {
                incoming["namespace_history"]["mix_graphs"].push_back(
                    {{"mix_graph_id", 2}, {"channel_ids", json::array({99})}});
                incoming["counters"]["next_graph_id"] = 3;
            } else {
                auto history = before["namespace_history"]["scores"][0];
                history["version_floor"] =
                    higher ? (score_exhausted ? std::numeric_limits<std::uint64_t>::max() : 20)
                           : current.session.score->find(2)->version;
                if (!higher) history["identities"]["parts"].push_back(99);
                incoming["namespace_history"]["scores"].push_back(std::move(history));
                incoming["counters"]["next_score_id"] = 3;
            }
            Directory directory;
            const auto path = directory.path / "exhausted-retirement.json";
            write_bytes(path, incoming.dump());
            const auto refused = import_workspace(current.session, path);
            REQUIRE_FALSE(refused);
            CHECK(refused.error().message.find("exhausted") != std::string::npos);
            CHECK(snapshot(current.session) == before);
        }
    }
}

TEST_CASE("Workspace Channel reservations follow the graph when its owning Score changes",
          "[mcp][workspace][identity][persistence]") {
    Fixture current;
    current.session.project->projects.at(1).reserved_channel_ids.insert(2);
    Fixture incoming{2};
    auto graph = incoming.session.mix->graphs.extract(2);
    graph.key() = 1;
    graph.mapped().id = MixGraphId{1};
    incoming.session.mix->graphs.insert(std::move(graph));
    incoming.session.project->projects.at(2).mix_graph_id = 1;
    Directory directory;
    const auto path = directory.path / "changed-owner.json";
    REQUIRE(save_workspace(incoming.session, path).committed);
    REQUIRE(open_workspace(current.session, path));
    REQUIRE(current.session.project->projects.contains(2));
    CHECK(current.session.project->projects.at(2).reserved_channel_ids.contains(2));
    McpServer server;
    register_score_tools(server, current.session.score);
    register_timbre_tools(server, current.session.timbre);
    register_mix_tools(server, current.session.mix);
    register_project_tools(server, current.session);
    const auto added = call(server, "score_add_part", {{"score_id", 2}, {"name", "Fresh"}});
    REQUIRE_FALSE(added.contains("error"));
    CHECK(added.at("channel_id") == 3);
}

TEST_CASE("Workspace publication rejects current or saved version exhaustion atomically",
          "[mcp][workspace][persistence][version]") {
    for (const bool incoming : {false, true}) {
        for (const bool project_revision : {false, true}) {
            INFO("incoming=" << incoming << " project_revision=" << project_revision);
            Fixture current;
            Fixture saved;
            auto& exhausted = incoming ? saved.session : current.session;
            if (project_revision)
                exhausted.project->projects.at(1).revision =
                    std::numeric_limits<std::uint64_t>::max();
            else
                exhausted.score->find(1)->version = std::numeric_limits<std::uint64_t>::max();
            Directory directory;
            const auto path = directory.path / "exhausted-version.json";
            REQUIRE(save_workspace(saved.session, path).committed);
            const auto before = snapshot(current.session);
            CHECK_FALSE(open_workspace(current.session, path));
            CHECK(snapshot(current.session) == before);
        }
    }
}

TEST_CASE("Pre-replacement save failures retain last valid bytes and clean partial temporary files",
          "[mcp][workspace][persistence]") {
    Fixture fixture;
    Directory directory;
    const auto path = directory.path / "workspace.json";
    REQUIRE(save_workspace(fixture.session, path).committed);
    const auto original = bytes(path);
    fixture.session.score->find(1)->metadata.title = "New authored content";
    for (const auto phase : {WorkspaceIoPhase::CreateTemporary,
                             WorkspaceIoPhase::AfterPartialWrite,
                             WorkspaceIoPhase::FileSync,
                             WorkspaceIoPhase::Replace}) {
        const auto saved = save_workspace(fixture.session, path, [phase](auto role, auto current) {
            return role == WorkspaceFileRole::Main && current == phase;
        });
        CHECK_FALSE(saved.success);
        CHECK_FALSE(saved.committed);
        CHECK(bytes(path) == original);
        CHECK(bytes(fs::path{path.string() + ".bak"}) == original);
        no_temporaries(directory);
    }
}

TEST_CASE("Post-replacement sync failure reports commit and explicit backup recovery never hides "
          "corrupt main",
          "[mcp][workspace][persistence]") {
    Fixture fixture;
    Directory directory;
    const auto path = directory.path / "workspace.json";
    REQUIRE(save_workspace(fixture.session, path).committed);
    const auto old_title = fixture.session.score->find(1)->metadata.title;
    fixture.session.score->find(1)->metadata.title = "New main";
    const auto saved = save_workspace(fixture.session, path, [](auto role, auto phase) {
        return role == WorkspaceFileRole::Main && phase == WorkspaceIoPhase::DirectorySync;
    });
    CHECK_FALSE(saved.success);
    CHECK(saved.committed);
    CHECK_FALSE(saved.durability_confirmed);
    CHECK_FALSE(saved.error.empty());
    const auto new_bytes = bytes(path);
    auto main = read_workspace(path);
    REQUIRE(main.has_value());
    CHECK(main->score.scores.at(1).metadata.title == "New main");
    fixture.session.score->find(1)->identity_reservations.parts.insert(PartId{25});
    fixture.session.project->projects.at(1).reserved_channel_ids.insert(26);
    const auto before_preview = snapshot(fixture.session);
    auto preview = recover_workspace_backup(fixture.session, path, false);
    REQUIRE(preview.has_value());
    CHECK((*preview)["preview"] == true);
    CHECK(snapshot(fixture.session) == before_preview);
    REQUIRE(recover_workspace_backup(fixture.session, path, true).has_value());
    CHECK(fixture.session.score->find(1)->metadata.title == old_title);
    CHECK(fixture.session.score->find(1)->identity_reservations.parts.contains(PartId{25}));
    CHECK(fixture.session.project->projects.at(1).reserved_channel_ids.contains(26));
    CHECK(bytes(path) == new_bytes);
    no_temporaries(directory);
}

TEST_CASE("Failed backup preparation prevents replacing the requested main file",
          "[mcp][workspace][persistence]") {
    Fixture fixture;
    Directory directory;
    const auto path = directory.path / "workspace.json";
    REQUIRE(save_workspace(fixture.session, path).committed);
    fixture.session.score->find(1)->metadata.title = "Second durable version";
    REQUIRE(save_workspace(fixture.session, path).committed);
    const auto previous_main = bytes(path);
    fixture.session.score->find(1)->metadata.title = "Third pending version";
    for (const auto phase : {WorkspaceIoPhase::CreateTemporary,
                             WorkspaceIoPhase::AfterPartialWrite,
                             WorkspaceIoPhase::FileSync,
                             WorkspaceIoPhase::Replace,
                             WorkspaceIoPhase::DirectorySync}) {
        const auto refused =
            save_workspace(fixture.session, path, [phase](auto role, auto current) {
                return role == WorkspaceFileRole::Backup && current == phase;
            });
        CHECK_FALSE(refused.committed);
        CHECK_FALSE(refused.success);
        CHECK(bytes(path) == previous_main);
        REQUIRE(read_workspace(fs::path{path.string() + ".bak"}).has_value());
        no_temporaries(directory);
    }
}

TEST_CASE("A corrupt previous main file cannot overwrite the last valid backup",
          "[mcp][workspace][persistence]") {
    Fixture fixture;
    Directory directory;
    const auto path = directory.path / "workspace.json";
    REQUIRE(save_workspace(fixture.session, path).committed);
    const auto original = bytes(path);
    fixture.session.score->find(1)->metadata.title = "Second version";
    REQUIRE(save_workspace(fixture.session, path).committed);
    const auto backup = fs::path{path.string() + ".bak"};
    CHECK(bytes(backup) == original);
    write_bytes(path, "{broken");
    const auto before = snapshot(fixture.session);
    CHECK_FALSE(open_workspace(fixture.session, path).has_value());
    CHECK(snapshot(fixture.session) == before);
    fixture.session.score->find(1)->metadata.title = "Explicitly repaired main";
    const auto repaired = save_workspace(fixture.session, path);
    REQUIRE(repaired.committed);
    CHECK_FALSE(repaired.backup_updated);
    CHECK(bytes(backup) == original);
    REQUIRE(read_workspace(path).has_value());
}

TEST_CASE("Workspace real path errors and injected exceptions have accurate commit status",
          "[mcp][workspace][persistence]") {
    Fixture fixture;
    Directory directory;
    const auto before = snapshot(fixture.session);
    const auto missing = directory.path / "missing" / "workspace.json";
    CHECK_FALSE(save_workspace(fixture.session, missing).committed);
    CHECK_FALSE(open_workspace(fixture.session, missing).has_value());
    CHECK(snapshot(fixture.session) == before);
    const auto path = directory.path / "workspace.json";
    REQUIRE(save_workspace(fixture.session, path).committed);
    const auto original = bytes(path);
    fixture.session.score->find(1)->metadata.title = "Exception candidate";
    const auto failed = save_workspace(fixture.session, path, [](auto role, auto phase) -> bool {
        if (role == WorkspaceFileRole::Main && phase == WorkspaceIoPhase::AfterPartialWrite)
            throw std::runtime_error("Injected write exception");
        return false;
    });
    CHECK_FALSE(failed.committed);
    CHECK(bytes(path) == original);
    const auto committed = save_workspace(fixture.session, path, [](auto role, auto phase) -> bool {
        if (role == WorkspaceFileRole::Main && phase == WorkspaceIoPhase::DirectorySync)
            throw std::runtime_error("Injected postcommit exception");
        return false;
    });
    CHECK(committed.committed);
    CHECK_FALSE(committed.success);
    REQUIRE(read_workspace(path).has_value());
    no_temporaries(directory);
}

TEST_CASE("Handlers registered before workspace open continue to use the replaced owning stores",
          "[mcp][workspace][persistence]") {
    Fixture fixture;
    Directory directory;
    const auto path = directory.path / "workspace.json";
    REQUIRE(save_workspace(fixture.session, path).committed);
    McpSession restarted;
    McpServer server;
    register_score_tools(server, restarted.score);
    register_timbre_tools(server, restarted.timbre);
    register_mix_tools(server, restarted.mix);
    register_project_tools(server, restarted);
    REQUIRE(!call(server, "workspace_open", {{"path", path.string()}}).contains("error"));
    REQUIRE(!call(server,
                  "set_parameter",
                  {{"profile_id", 1}, {"path", "source.filter.cutoff"}, {"value", 5100}})
                 .contains("error"));
    CHECK(*get_parameter(*restarted.timbre->find(1), "source.filter.cutoff") ==
          Catch::Approx(5100));
    REQUIRE(!call(server, "score_undo", {{"score_id", 1}}).contains("error"));
    CHECK(*get_parameter(*restarted.timbre->find(1), "source.filter.cutoff") ==
          Catch::Approx(3800));
    const auto saved = call(server, "workspace_save", {{"path", path.string()}});
    CHECK(saved["committed"] == true);
    CHECK(call(server, "workspace_import", {{"path", path.string()}}).contains("error"));
    CHECK(call(server, "workspace_recover_backup", {{"path", path.string()}})["preview"] == true);
}
