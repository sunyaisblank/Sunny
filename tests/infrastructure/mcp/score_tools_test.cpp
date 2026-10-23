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
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <sunny/core/score/serialization.hpp>
#include <sunny/infrastructure/mcp/score_tools.hpp>
#include <sunny/infrastructure/mcp/server.hpp>
#include <tuple>
#include <vector>

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

    ScoreFixture() { register_score_tools(server, session); }

    json call(const std::string& name, const json& arguments) {
        return call_tool(server, name, arguments);
    }
};

json pitch(const char* letter, int accidental, int octave) {
    return {{"letter", letter}, {"accidental", accidental}, {"octave", octave}};
}

json fraction(int numerator, int denominator) {
    return {{"n", numerator}, {"d", denominator}};
}

/// A note as the serialised Score stores it: bar, letter index, accidental,
/// octave and the optional dynamic enum.
struct StoredNote {
    int bar;
    int letter;
    int accidental;
    int octave;
    std::optional<int> dynamic;
    bool operator==(const StoredNote&) const = default;
};

std::vector<StoredNote> stored_notes(const json& document, std::size_t part_index) {
    std::vector<StoredNote> notes;
    for (const auto& measure : document["parts"][part_index]["measures"]) {
        for (const auto& voice : measure["voices"]) {
            for (const auto& event : voice["events"]) {
                if (event["type"] != "note_group") continue;
                for (const auto& note : event["notes"]) {
                    std::optional<int> dynamic;
                    if (note.contains("dynamic")) dynamic = note["dynamic"].get<int>();
                    notes.push_back({measure["bar_number"].get<int>(),
                                     note["pitch"]["letter"].get<int>(),
                                     note["pitch"]["accidental"].get<int>(),
                                     note["pitch"]["octave"].get<int>(),
                                     dynamic});
                }
            }
        }
    }
    return notes;
}

json insert_quarter(ScoreFixture& fixture,
                    const json& score_id,
                    const json& part_id,
                    int bar,
                    int quarter_index,
                    const json& note_pitch) {
    return fixture.call("score_insert_note",
                        {{"score_id", score_id},
                         {"part_id", part_id},
                         {"bar", bar},
                         {"offset", fraction(quarter_index, 4)},
                         {"pitch", note_pitch},
                         {"duration", fraction(1, 4)}});
}

} // anonymous namespace

TEST_CASE("Score creation and reduction use checked global high-water identities",
          "[mcp][score][identity][allocation]") {
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    for (const bool reduction : {false, true}) {
        INFO("reduction=" << reduction);
        ScoreFixture fixture;
        const json specification{{"total_bars", 2},
                                 {"parts", {{{"name", "Piano"}, {"instrument_type", 0}}}}};
        REQUIRE_FALSE(fixture.call("score_create", specification).contains("error"));
        const auto allocate = [&] {
            return reduction ? fixture.call("score_get_reduction", {{"score_id", 1}})
                             : fixture.call("score_create", specification);
        };
        const auto original = sunny::core::score_to_json(*fixture.session->find(1));
        for (const auto counter : {std::uint64_t{0}, std::uint64_t{1}, maximum}) {
            fixture.session->next_score_id = counter;
            REQUIRE(allocate().contains("error"));
            CHECK(fixture.session->next_score_id == counter);
            CHECK(fixture.session->scores.size() == 1);
            CHECK(sunny::core::score_to_json(*fixture.session->find(1)) == original);
        }
        auto imported = *fixture.session->find(1);
        imported.id = sunny::core::ScoreId{73};
        fixture.session->scores.emplace(73, imported);
        fixture.session->next_score_id = 70;
        CHECK(allocate().contains("error"));
        CHECK(fixture.session->next_score_id == 70);
        CHECK(fixture.session->scores.size() == 2);
        fixture.session->next_score_id = maximum - 1;
        const auto last = allocate();
        REQUIRE_FALSE(last.contains("error"));
        CHECK(last.at("score_id") == maximum - 1);
        CHECK(fixture.session->next_score_id == maximum);
        CHECK(allocate().contains("error"));
        CHECK(fixture.session->scores.size() == 3);
    }
}

TEST_CASE("Failed Score candidates consume no global identities",
          "[mcp][score][identity][allocation]") {
    ScoreFixture fixture;
    const json specification{{"total_bars", 2},
                             {"parts", {{{"name", "Piano"}, {"instrument_type", 0}}}}};
    REQUIRE_FALSE(fixture.call("score_create", specification).contains("error"));
    const auto before = sunny::core::score_to_json(*fixture.session->find(1));
    auto invalid = specification;
    invalid["total_bars"] = 0;
    CHECK(fixture.call("score_create", invalid).contains("error"));
    CHECK(fixture.call("score_get_reduction", {{"score_id", 1}, {"view_type", "unknown"}})
              .contains("error"));
    CHECK(fixture.session->next_score_id == 2);
    CHECK(fixture.session->scores.size() == 1);
    CHECK(sunny::core::score_to_json(*fixture.session->find(1)) == before);
}

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

// =============================================================================
// Issue #11: ranges, region end_bar, tempo, doubling, undo
// =============================================================================

TEST_CASE("an MCP piano part takes the Appendix A range and validates C2 to C7",
          "[mcp][score][validation][regression]") {
    // Score IR Appendix A: piano sounds A0..C8, so C2, C4 and C7 are playable
    // and the part must validate without any diagnostic.
    ScoreFixture fixture;
    const auto created = fixture.call(
        "score_create",
        {{"total_bars", 1}, {"parts", json::array({{{"name", "Pno"}, {"instrument_type", 47}}})}});
    const auto score_id = created["score_id"];
    const auto part_id = created["part_ids"][0];
    REQUIRE(insert_quarter(fixture, score_id, part_id, 1, 0, pitch("C", 0, 2)).contains("ok"));
    REQUIRE(insert_quarter(fixture, score_id, part_id, 1, 1, pitch("C", 0, 4)).contains("ok"));
    REQUIRE(insert_quarter(fixture, score_id, part_id, 1, 2, pitch("C", 0, 7)).contains("ok"));

    // Melodic-style warnings (leaps, missing harmony, missing preset) are
    // unrelated to range; the contract is that no range rule and no error fires.
    const auto validation = fixture.call("score_validate", {{"score_id", score_id}});
    INFO(validation.dump());
    for (const auto& diagnostic : validation["diagnostics"]) {
        CHECK(diagnostic["rule"] != "M1");
        CHECK(diagnostic["rule"] != "M2");
        CHECK(diagnostic["severity"] != "error");
    }
    CHECK(validation["midi_compilable"] == true);

    const auto document = fixture.call("score_get_json", {{"score_id", score_id}});
    const auto& range = document["parts"][0]["definition"]["range"];
    CHECK(range["absolute_low"] == json{{"letter", 5}, {"accidental", 0}, {"octave", 0}});
    CHECK(range["absolute_high"] == json{{"letter", 0}, {"accidental", 0}, {"octave", 8}});
}

TEST_CASE("an MCP part added later also takes its Appendix A range and transposition",
          "[mcp][score][validation][regression]") {
    // Clarinet in B-flat (instrument 16): sounding D3..B-flat 6, transposition
    // -2 semitones (a major second down).
    ScoreFixture fixture;
    const auto created = fixture.call(
        "score_create",
        {{"total_bars", 1}, {"parts", json::array({{{"name", "Pno"}, {"instrument_type", 47}}})}});
    const auto added =
        fixture.call("score_add_part",
                     {{"score_id", created["score_id"]}, {"name", "Cl"}, {"instrument_type", 16}});
    REQUIRE(added.contains("part_id"));
    const auto document = fixture.call("score_get_json", {{"score_id", created["score_id"]}});
    const auto& definition = document["parts"].back()["definition"];
    CHECK(definition["transposition"] == -2);
    CHECK(definition["range"]["absolute_low"] ==
          json{{"letter", 1}, {"accidental", 0}, {"octave", 3}});
    CHECK(definition["range"]["absolute_high"] ==
          json{{"letter", 6}, {"accidental", -1}, {"octave", 6}});
}

TEST_CASE("MCP region end_bar is inclusive", "[mcp][score][region][regression]") {
    // {start_bar 1, end_bar 1} names bar 1 alone; {2, 2} names bar 2 alone.
    ScoreFixture fixture;
    const auto created = fixture.call(
        "score_create",
        {{"total_bars", 2}, {"parts", json::array({{{"name", "Pno"}, {"instrument_type", 47}}})}});
    const auto score_id = created["score_id"];
    const auto part_id = created["part_ids"][0];
    REQUIRE(insert_quarter(fixture, score_id, part_id, 1, 0, pitch("C", 0, 4)).contains("ok"));
    REQUIRE(insert_quarter(fixture, score_id, part_id, 2, 0, pitch("D", 0, 4)).contains("ok"));

    constexpr int ff = 7;
    constexpr int pp = 2;
    const auto first = fixture.call(
        "score_set_dynamics",
        {{"score_id", score_id}, {"region", {{"start_bar", 1}, {"end_bar", 1}}}, {"level", ff}});
    REQUIRE(first.contains("ok"));
    auto notes = stored_notes(fixture.call("score_get_json", {{"score_id", score_id}}), 0);
    REQUIRE(notes.size() == 2);
    CHECK(notes[0].dynamic == ff);
    CHECK_FALSE(notes[1].dynamic.has_value());

    const auto second = fixture.call(
        "score_set_dynamics",
        {{"score_id", score_id}, {"region", {{"start_bar", 2}, {"end_bar", 2}}}, {"level", pp}});
    REQUIRE(second.contains("ok"));
    notes = stored_notes(fixture.call("score_get_json", {{"score_id", score_id}}), 0);
    CHECK(notes[0].dynamic == ff);
    CHECK(notes[1].dynamic == pp);

    const auto listed =
        fixture.server.process_request({{"jsonrpc", "2.0"}, {"method", "tools/list"}, {"id", 1}});
    for (const auto& tool : listed["result"]["tools"]) {
        const auto& properties = tool["inputSchema"]["properties"];
        if (!properties.contains("region")) continue;
        INFO(tool["name"]);
        const auto& region = properties["region"];
        const auto description = region["description"].get<std::string>();
        if (region.contains("properties") && region["properties"].contains("start")) {
            REQUIRE(region["properties"].contains("end"));
            CHECK(description.find("Half-open") != std::string::npos);
        } else {
            // Existing bar-region schemas describe their fields in prose.
            CHECK(description.find("inclusive") != std::string::npos);
        }
    }
}

TEST_CASE("MCP score_create requires at least one part and says so",
          "[mcp][score][schema][regression]") {
    // Score IR rule S0: a score has at least one Part. The schema must publish
    // parts as required, and an empty list must be refused by name rather than
    // as an anonymous mutation error.
    ScoreFixture fixture;
    const auto missing = fixture.call("score_create", json::object());
    REQUIRE(missing.contains("error"));
    CHECK(missing["error"].get<std::string>().find("parts") != std::string::npos);
    const auto empty = fixture.call("score_create", {{"parts", json::array()}});
    REQUIRE(empty.contains("error"));
    CHECK(empty["error"].get<std::string>().find("at least one part") != std::string::npos);
}

TEST_CASE("MCP fractional tempo is stored exactly", "[mcp][score][tempo][regression]") {
    // 92.5 quarter notes per minute: 60 000 000 / 92.5 = 648 648.65 us,
    // rounded to the SMF tempo word 648 649.
    ScoreFixture fixture;
    const auto created =
        fixture.call("score_create",
                     {{"total_bars", 1},
                      {"bpm", 92.5},
                      {"parts", json::array({{{"name", "Pno"}, {"instrument_type", 47}}})}});
    const auto midi = fixture.call("score_compile_to_midi", {{"score_id", created["score_id"]}});
    REQUIRE(midi["tempos"].size() == 1);
    CHECK(midi["tempos"][0]["microseconds_per_beat"] == 648649);
}

TEST_CASE("MCP doubling spells source plus a diatonic interval",
          "[mcp][score][transposition][regression]") {
    // D4 up a perfect fifth is A4; up a perfect octave it is D5. B-sharp 3 up
    // a perfect octave is B-sharp 4. Letter names advance by 4 and 7 steps.
    ScoreFixture fixture;
    const auto created =
        fixture.call("score_create",
                     {{"total_bars", 1},
                      {"parts",
                       json::array({{{"name", "Src"}, {"instrument_type", 0}},
                                    {{"name", "Fifth"}, {"instrument_type", 0}},
                                    {{"name", "Octave"}, {"instrument_type", 0}}})}});
    const auto score_id = created["score_id"];
    const auto source = created["part_ids"][0];
    REQUIRE(insert_quarter(fixture, score_id, source, 1, 0, pitch("D", 0, 4)).contains("ok"));
    REQUIRE(insert_quarter(fixture, score_id, source, 1, 1, pitch("B", 1, 3)).contains("ok"));

    const json region{{"start_bar", 1}, {"end_bar", 1}};
    REQUIRE(fixture
                .call("score_double_part",
                      {{"score_id", score_id},
                       {"region", region},
                       {"source_part", source},
                       {"target_part", created["part_ids"][1]},
                       {"interval", 7}})
                .contains("ok"));
    REQUIRE(fixture
                .call("score_double_part",
                      {{"score_id", score_id},
                       {"region", region},
                       {"source_part", source},
                       {"target_part", created["part_ids"][2]},
                       {"interval", 12}})
                .contains("ok"));

    const auto document = fixture.call("score_get_json", {{"score_id", score_id}});
    const auto fifth = stored_notes(document, 1);
    REQUIRE(fifth.size() == 2);
    CHECK(fifth[0] == StoredNote{1, 5, 0, 4, std::nullopt}); // A4
    CHECK(fifth[1] == StoredNote{1, 3, 2, 4, std::nullopt}); // F double-sharp 4
    const auto octave = stored_notes(document, 2);
    REQUIRE(octave.size() == 2);
    CHECK(octave[0] == StoredNote{1, 1, 0, 5, std::nullopt}); // D5
    CHECK(octave[1] == StoredNote{1, 6, 1, 4, std::nullopt}); // B-sharp 4
}

TEST_CASE("MCP doubling copies tuplet passages into silent target parts",
          "[mcp][score][transposition][regression]") {
    // C4 D4 E4 as triplet eighths over beat 1, then a quarter F4. Doubled up a
    // perfect fifth into an empty part they become G4 A4 B4 and C5, and the
    // copy keeps its 3:2 grouping, so MusicXML shows three eighths under a
    // time modification exactly as the source does.
    ScoreFixture fixture;
    const auto created =
        fixture.call("score_create",
                     {{"total_bars", 1},
                      {"parts",
                       json::array({{{"name", "Src"}, {"instrument_type", 47}},
                                    {{"name", "Copy"}, {"instrument_type", 47}}})}});
    const auto score_id = created["score_id"];
    const auto source = created["part_ids"][0];
    auto melody = json::array();
    const char* letters[] = {"C", "D", "E"};
    for (int unit = 0; unit < 3; ++unit)
        melody.push_back({{"position", {{"bar", 1}, {"beat_n", unit}, {"beat_d", 12}}},
                          {"pitch", pitch(letters[unit], 0, 4)},
                          {"duration", fraction(1, 12)}});
    melody.push_back({{"position", {{"bar", 1}, {"beat_n", 1}, {"beat_d", 4}}},
                      {"pitch", pitch("F", 0, 4)},
                      {"duration", fraction(1, 4)}});
    REQUIRE(fixture
                .call("score_write_melody",
                      {{"score_id", score_id}, {"part_id", source}, {"melody", melody}})
                .contains("ok"));

    const auto doubled = fixture.call("score_double_part",
                                      {{"score_id", score_id},
                                       {"region", {{"start_bar", 1}, {"end_bar", 1}}},
                                       {"source_part", source},
                                       {"target_part", created["part_ids"][1]},
                                       {"interval", 7}});
    REQUIRE(doubled.contains("ok"));

    const auto document = fixture.call("score_get_json", {{"score_id", score_id}});
    const auto copy = stored_notes(document, 1);
    REQUIRE(copy.size() == 4);
    CHECK(copy[0] == StoredNote{1, 4, 0, 4, std::nullopt}); // G4
    CHECK(copy[1] == StoredNote{1, 5, 0, 4, std::nullopt}); // A4
    CHECK(copy[2] == StoredNote{1, 6, 0, 4, std::nullopt}); // B4
    CHECK(copy[3] == StoredNote{1, 0, 0, 5, std::nullopt}); // C5

    const auto xml = fixture.call("score_compile_to_musicxml", {{"score_id", score_id}});
    const auto text = xml.dump();
    std::size_t modifications = 0;
    for (auto at = text.find("<time-modification>"); at != std::string::npos;
         at = text.find("<time-modification>", at + 1))
        ++modifications;
    CHECK(modifications == 6); // three per part
}

TEST_CASE("MCP undo restores the prior document and redo reapplies it",
          "[mcp][score][undo][regression]") {
    ScoreFixture fixture;
    const auto created = fixture.call(
        "score_create",
        {{"total_bars", 1}, {"parts", json::array({{{"name", "Pno"}, {"instrument_type", 47}}})}});
    const auto score_id = created["score_id"];
    const auto part_id = created["part_ids"][0];
    const auto before = fixture.call("score_get_json", {{"score_id", score_id}});

    REQUIRE(insert_quarter(fixture, score_id, part_id, 1, 0, pitch("E", 0, 4)).contains("ok"));
    const auto after = fixture.call("score_get_json", {{"score_id", score_id}});
    REQUIRE(stored_notes(after, 0).size() == 1);

    const auto undone = fixture.call("score_undo", {{"score_id", score_id}});
    REQUIRE(undone.contains("ok"));
    CHECK(undone["can_undo"] == false);
    CHECK(undone["can_redo"] == true);
    auto restored = fixture.call("score_get_json", {{"score_id", score_id}});
    CHECK(restored["parts"] == before["parts"]);

    const auto redone = fixture.call("score_redo", {{"score_id", score_id}});
    REQUIRE(redone.contains("ok"));
    restored = fixture.call("score_get_json", {{"score_id", score_id}});
    CHECK(restored["parts"] == after["parts"]);

    REQUIRE(fixture.call("score_undo", {{"score_id", score_id}}).contains("ok"));
    CHECK(fixture.call("score_undo", {{"score_id", score_id}}).contains("error"));
}
