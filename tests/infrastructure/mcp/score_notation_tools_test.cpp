/** @file score_notation_tools_test.cpp
 *  @brief Literal exact authoring and export contracts through MCP.
 */
#include <catch2/catch_test_macros.hpp>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/score/time.hpp>
#include <sunny/core/score/tuplets.hpp>
#include <sunny/infrastructure/formats/score_to_musicxml.hpp>
#include <sunny/infrastructure/mcp/score_notation_tools.hpp>

using namespace sunny::core;
using namespace sunny::infrastructure;
using json = nlohmann::json;

namespace {

struct NotationFixture {
    McpServer server;
    std::shared_ptr<ScoreSession> session = std::make_shared<ScoreSession>();

    explicit NotationFixture(std::uint32_t bars = 2) {
        ScoreSpec spec;
        spec.title = "Exact authoring";
        spec.total_bars = bars;
        spec.bpm = 120.0;
        spec.key_root = SpelledPitch{0, 0, 4};
        PartDefinition part;
        part.name = "Piano";
        part.abbreviation = "Pno.";
        part.instrument_type = InstrumentType::Piano;
        part.rendering.midi_channel = 1;
        spec.parts = {part};
        auto created = create_score(spec);
        REQUIRE(created);
        session->scores.emplace(1, std::move(*created));
        session->undo_stacks.emplace(1, UndoStack{});
        register_score_notation_tools(server, session);
    }

    Score& score() { return session->scores.at(1); }

    json call(const std::string& name, const json& arguments) {
        auto response =
            server.process_request({{"jsonrpc", "2.0"},
                                    {"id", 1},
                                    {"method", "tools/call"},
                                    {"params", {{"name", name}, {"arguments", arguments}}}});
        INFO(response.dump());
        REQUIRE(response.contains("result"));
        return json::parse(response.at("result").at("content").at(0).at("text").get<std::string>());
    }
};

json fraction(std::int64_t numerator, std::int64_t denominator) {
    return {{"n", numerator}, {"d", denominator}};
}

json position(std::uint32_t bar, std::int64_t numerator = 0, std::int64_t denominator = 1) {
    return {{"bar", bar}, {"beat_n", numerator}, {"beat_d", denominator}};
}

json first_bar_region() {
    return {{"start", position(1)}, {"end", position(2)}};
}

void place_triplet_phrase(Score& score, bool tied) {
    auto& events = score.parts[0].measures[0].voices[0].events;
    events.clear();
    for (std::uint64_t index = 0; index < 3; ++index) {
        Note note;
        note.pitch = index < 2 ? SpelledPitch{0, 0, 4} : SpelledPitch{2, 0, 4};
        note.velocity = VelocityValue{{}, 80};
        note.tie_forward = tied && index == 0;
        NoteGroup group;
        group.duration = Beat{1, 12};
        group.notes = {note};
        events.push_back(
            Event{EventId{1600000 + index}, Beat{static_cast<std::int64_t>(index), 12}, group});
    }
    events.push_back(Event{EventId{1600010}, Beat{1, 4}, RestEvent{Beat{3, 4}, true}});
    REQUIRE(is_compilable(score));
}

json tuplet_arguments() {
    return {{"score_id", 1},
            {"event_ids", {1600002, 1600000, 1600001}},
            {"actual", 3},
            {"normal", 2},
            {"normal_type", fraction(1, 8)},
            {"scaled_allocation", fraction(1, 4)}};
}

std::size_t occurrences(const std::string& text, std::string_view fragment) {
    std::size_t count = 0, offset = 0;
    while ((offset = text.find(fragment, offset)) != std::string::npos) {
        ++count;
        offset += fragment.size();
    }
    return count;
}

} // namespace

TEST_CASE("MCP exact tempo replacement retains rational payload and incoming ramp semantics",
          "[mcp][score][notation][tempo]") {
    NotationFixture fixture;
    const json initial{{"position", position(1)},
                       {"bpm", fraction(185, 2)},
                       {"beat_unit", "quarter"},
                       {"transition", "immediate"}};
    json target{{"position", position(2)},
                {"bpm", fraction(120, 1)},
                {"beat_unit", "quarter"},
                {"transition", "linear"},
                {"linear_duration", fraction(1, 1)}};
    const auto version = fixture.score().version;
    const auto changed =
        fixture.call("score_set_tempo_map", {{"score_id", 1}, {"entries", {initial, target}}});
    REQUIRE(changed.at("ok") == true);
    CHECK(changed.at("version") == version + 1);
    CHECK(fixture.score().tempo_map[0].bpm == PositiveRational{185, 2});
    CHECK(fixture.score().tempo_map[1].linear_duration == Beat::one());
    const auto midpoint = effective_quarter_tempo_at(
        {1, Beat{1, 2}}, fixture.score().tempo_map, fixture.score().time_map);
    REQUIRE(midpoint);
    CHECK(*midpoint == PositiveRational{425, 4}); // (92.5 + 120)/2 = 106.25
    CHECK(fixture.session->undo_stacks.at(1).undo_entries.size() == 1);

    const auto before = score_to_json(fixture.score());
    target["linear_duration"] = fraction(3, 4);
    CHECK(fixture.call("score_set_tempo_map", {{"score_id", 1}, {"entries", {initial, target}}})
              .contains("error"));
    target.erase("linear_duration");
    CHECK(fixture.call("score_set_tempo_map", {{"score_id", 1}, {"entries", {initial, target}}})
              .contains("error"));
    target["bpm"] = fraction(-1, 1);
    CHECK(fixture.call("score_set_tempo_map", {{"score_id", 1}, {"entries", {initial, target}}})
              .contains("error"));
    CHECK(score_to_json(fixture.score()) == before);
    CHECK(fixture.session->undo_stacks.at(1).undo_entries.size() == 1);
}

TEST_CASE("MCP metric modulation requires the exact declared unit relation",
          "[mcp][score][notation][tempo]") {
    NotationFixture fixture;
    const json initial{{"position", position(1)},
                       {"bpm", fraction(120, 1)},
                       {"beat_unit", "quarter"},
                       {"transition", "immediate"}};
    json modulation{{"position", position(2)},
                    {"bpm", fraction(120, 1)},
                    {"beat_unit", "dotted_quarter"},
                    {"transition", "metric_modulation"},
                    {"old_unit", "quarter"},
                    {"new_unit", "dotted_quarter"}};
    REQUIRE(
        fixture.call("score_set_tempo_map", {{"score_id", 1}, {"entries", {initial, modulation}}})
            .at("ok") == true);
    const auto tempo = effective_quarter_tempo_at(
        {2, Beat::zero()}, fixture.score().tempo_map, fixture.score().time_map);
    REQUIRE(tempo);
    CHECK(*tempo == PositiveRational{180, 1});
    const auto before = score_to_json(fixture.score());
    modulation["bpm"] = fraction(121, 1);
    CHECK(fixture.call("score_set_tempo_map", {{"score_id", 1}, {"entries", {initial, modulation}}})
              .contains("error"));
    CHECK(score_to_json(fixture.score()) == before);
}

TEST_CASE("MCP tuplet creation/removal preserves timing and reserves the retired identity",
          "[mcp][score][notation][tuplet][identity]") {
    NotationFixture fixture(1);
    place_triplet_phrase(fixture.score(), false);
    const auto arguments = tuplet_arguments();
    const auto created = fixture.call("score_create_tuplet_group", arguments);
    REQUIRE(created.at("ok") == true);
    CHECK(created.at("tuplet_id") == 1);
    const auto with_group = score_to_json(fixture.score());
    auto repeated = arguments;
    repeated["event_ids"] = {1600000, 1600000, 1600002};
    CHECK(fixture.call("score_create_tuplet_group", repeated).contains("error"));
    CHECK(score_to_json(fixture.score()) == with_group);
    REQUIRE(
        fixture.call("score_remove_tuplet_group", {{"score_id", 1}, {"tuplet_id", 1}}).at("ok") ==
        true);
    const auto& events = fixture.score().parts[0].measures[0].voices[0].events;
    for (std::size_t index = 0; index < 3; ++index) {
        CHECK(events[index].id == EventId{1600000 + index});
        CHECK(events[index].duration() == Beat{1, 12});
        CHECK_FALSE(event_tuplet_context(events[index]));
    }
    CHECK(fixture.score().identity_reservations.tuplets.contains(TupletId{1}));
    REQUIRE(undo(fixture.score(), fixture.session->undo_stacks.at(1)));
    REQUIRE(undo(fixture.score(), fixture.session->undo_stacks.at(1)));
    const auto next = fixture.call("score_create_tuplet_group", arguments);
    REQUIRE(next.at("ok") == true);
    CHECK(next.at("tuplet_id") == 2);
}

TEST_CASE("all seven region wrappers reach the shared Score authoring boundary and keep one "
          "history entry",
          "[mcp][score][notation][region][history]") {
    for (const auto* name : {"score_delete_region",
                             "score_copy_region",
                             "score_move_region",
                             "score_retrograde_region",
                             "score_invert_region",
                             "score_augment_region",
                             "score_diminish_region"}) {
        INFO(name);
        NotationFixture fixture;
        Note note;
        note.pitch = SpelledPitch{2, 0, 4}; // E4
        note.velocity = VelocityValue{{}, 80};
        REQUIRE(insert_note(
            fixture.score(), fixture.score().parts[0].id, 1, 0, Beat::zero(), note, Beat{1, 4}));
        McpDocumentDomain observed = McpDocumentDomain::None;
        fixture.server.set_tool_executor([&](McpDocumentDomain domain,
                                             const std::string&,
                                             const json& params,
                                             const McpToolHandler& handler) {
            observed = domain;
            return handler(params);
        });
        json arguments{{"score_id", 1}, {"region", first_bar_region()}};
        const std::string operation{name};
        if (operation == "score_copy_region" || operation == "score_move_region")
            arguments["destination"] = position(2);
        if (operation == "score_invert_region")
            arguments["axis"] = {{"letter", "C"}, {"accidental", 0}, {"octave", 4}};
        if (operation == "score_augment_region" || operation == "score_diminish_region")
            arguments["factor"] = fraction(2, 1);
        const auto version = fixture.score().version;
        const auto result = fixture.call(name, arguments);
        REQUIRE(result.at("ok") == true);
        CHECK(observed == McpDocumentDomain::Score);
        CHECK(fixture.score().version == version + 1);
        CHECK(fixture.session->undo_stacks.at(1).undo_entries.size() == 1);
        const auto midi = compile_to_midi(fixture.score());
        REQUIRE(midi);
        if (operation == "score_delete_region")
            CHECK(midi->midi.notes.empty());
        else {
            REQUIRE_FALSE(midi->midi.notes.empty());
            if (operation == "score_copy_region") CHECK(midi->midi.notes.size() == 2);
            if (operation == "score_move_region") CHECK(midi->midi.notes[0].tick == 1920);
            if (operation == "score_retrograde_region") CHECK(midi->midi.notes[0].tick == 1440);
            if (operation == "score_invert_region")
                CHECK(midi->midi.notes[0].note == 56); // A-flat3
            if (operation == "score_augment_region")
                CHECK(midi->midi.notes[0].duration_ticks == 960);
            if (operation == "score_diminish_region")
                CHECK(midi->midi.notes[0].duration_ticks == 240);
        }
    }
}

TEST_CASE(
    "MCP tied triplet authoring and instrument assignment export literal sound and written pitches",
    "[mcp][score][notation][tuplet][tie][instrument][export]") {
    NotationFixture fixture(1);
    place_triplet_phrase(fixture.score(), true);
    REQUIRE(fixture.call("score_create_tuplet_group", tuplet_arguments()).at("ok") == true);
    REQUIRE(
        assign_instrument(fixture.score(), fixture.score().parts[0].id, InstrumentType::Clarinet));
    const auto midi = compile_to_midi(fixture.score(), 480);
    REQUIRE(midi);
    REQUIRE(midi->midi.notes.size() == 2);
    CHECK(midi->midi.notes[0].tick == 0);
    CHECK(midi->midi.notes[0].duration_ticks == 320); // two eighth-triplet allocations, 1/6 whole
    CHECK(midi->midi.notes[0].note == 60); // concert C4, unchanged by clarinet assignment
    CHECK(midi->midi.notes[1].tick == 320);
    CHECK(midi->midi.notes[1].duration_ticks == 160);
    CHECK(midi->midi.notes[1].note == 64); // concert E4
    const auto xml = sunny::infrastructure::formats::compile_score_to_musicxml(fixture.score());
    REQUIRE(xml);
    CHECK(occurrences(xml->xml, "<step>D</step>") == 2); // written D4 is sounding C4
    CHECK(occurrences(xml->xml, "<step>F</step>") == 1); // written F-sharp4 is sounding E4
    CHECK(occurrences(xml->xml, "<alter>1</alter>") == 1);
    CHECK(occurrences(xml->xml, "<octave>4</octave>") == 3);
    CHECK(xml->xml.find("<diatonic>-1</diatonic>") != std::string::npos);
    CHECK(xml->xml.find("<chromatic>-2</chromatic>") != std::string::npos);
    CHECK(occurrences(xml->xml, "<actual-notes>3</actual-notes>") == 3);
    CHECK(occurrences(xml->xml, "<normal-notes>2</normal-notes>") == 3);
    CHECK(occurrences(xml->xml, "<tie type=\"start\"") == 1);
    CHECK(occurrences(xml->xml, "<tie type=\"stop\"") == 1);
    CHECK(occurrences(xml->xml, "<tuplet type=\"start\"") == 1);
    CHECK(occurrences(xml->xml, "<tuplet type=\"stop\"") == 1);
}

TEST_CASE("MCP malformed exact fractions and narrowed counts publish no candidate or identity",
          "[mcp][score][notation][atomicity]") {
    NotationFixture fixture(1);
    place_triplet_phrase(fixture.score(), false);
    const auto before = score_to_json(fixture.score());
    auto arguments = tuplet_arguments();
    arguments["actual"] = 256;
    CHECK(fixture.call("score_create_tuplet_group", arguments).contains("error"));
    arguments = tuplet_arguments();
    arguments["scaled_allocation"] = fraction(1, 0);
    CHECK(fixture.call("score_create_tuplet_group", arguments).contains("error"));
    arguments = tuplet_arguments();
    arguments["event_ids"] = {1600000, 1600001, 9999999};
    CHECK(fixture.call("score_create_tuplet_group", arguments).contains("error"));
    CHECK(score_to_json(fixture.score()) == before);
    CHECK(fixture.session->undo_stacks.at(1).undo_entries.empty());
    REQUIRE(fixture.call("score_create_tuplet_group", tuplet_arguments()).at("tuplet_id") == 1);
}
