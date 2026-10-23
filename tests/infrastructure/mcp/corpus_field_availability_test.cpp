/** MCP field counts use the same eligible observations as core aggregation. */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/core/corpus/serialization.hpp>
#include <sunny/core/corpus/workflows.hpp>
#include <sunny/infrastructure/mcp/corpus_tools.hpp>

using namespace sunny::core;
using namespace sunny::infrastructure;
using json = nlohmann::json;

namespace {
json field_call(McpServer& server, const std::string& name, const json& args) {
    const auto reply = server.process_request({{"jsonrpc", "2.0"},
                                               {"id", 1},
                                               {"method", "tools/call"},
                                               {"params", {{"name", name}, {"arguments", args}}}});
    REQUIRE(reply.contains("result"));
    return json::parse(reply["result"]["content"][0]["text"].get<std::string>());
}

std::shared_ptr<CorpusSession> field_session(const std::vector<WorkAnalysis>& analyses) {
    auto session = std::make_shared<CorpusSession>();
    session->corpus.composers[1] =
        create_composer_profile(ComposerProfileId{1}, "Field observations");
    for (std::size_t index = 0; index < analyses.size(); ++index) {
        IngestedWork work;
        work.id = IngestedWorkId{index + 1};
        work.analysis_complete = true;
        work.analysis = analyses[index];
        session->corpus.works[work.id.value] = work;
        REQUIRE(assign_work_to_composer(session->corpus, work.id, ComposerProfileId{1}));
    }
    return session;
}
} // namespace

TEST_CASE("MCP profile fields distinguish no observations from measured zero",
          "[mcp][corpus][field-evidence]") {
    WorkAnalysis two;
    VoiceMelodicAnalysis voice;
    voice.part_id = PartId{1};
    voice.voice_index = 0;
    voice.note_count = 2;
    voice.range_low = 60;
    voice.range_high = 62;
    voice.conjunct_proportion = 1.0f;
    voice.interval_distribution = {{2, 1}};
    two.melodic_analysis.per_voice_analysis = {voice};
    two.evidence["melodic"] = {
        AnalysisEvidenceKind::Heuristic, "Two literal attacks", std::nullopt, {}, 2};
    two.evidence["rhythmic"] = {AnalysisEvidenceKind::Heuristic,
                                "Notated rhythmic inventory",
                                std::nullopt,
                                {"rubato_degree"},
                                1};
    auto one = two;
    auto& singleton = one.melodic_analysis.per_voice_analysis[0];
    singleton.note_count = 1;
    singleton.range_high = 60;
    singleton.conjunct_proportion = 0.0f;
    singleton.interval_distribution.clear();
    one.evidence["melodic"].observations = 1;
    one.evidence["melodic"].unavailable_fields = {
        "lanes_with_fewer_than_two_attacks.conjunct_proportion"};
    auto session = field_session({two, one});
    McpServer server;
    register_corpus_tools(server, session);
    const auto response = field_call(server, "query_style_profile", {{"composer_id", 1}});
    CHECK(response["profile"]["melodic_profile"]["conjunct_proportion"] == 1.0f);
    const auto conjunct = response["availability"]["melodic"]["fields"]["conjunct_proportion"];
    CHECK(conjunct["computed_works"] == 1);
    CHECK(conjunct["unavailable_works"] == 1);
    CHECK(conjunct["unqualified_works"] == 0);
    CHECK(conjunct["contributing_works"] == 1);
    CHECK(conjunct["available"] == true);
    CHECK(conjunct["status"] == "computed");
    const auto rubato = response["availability"]["rhythmic"]["fields"]["rubato_tendency"];
    CHECK(rubato["computed_works"] == 0);
    CHECK(rubato["unavailable_works"] == 2);
    CHECK(rubato["contributing_works"] == 0);
    CHECK(rubato["available"] == false);
    CHECK(rubato["fully_qualified"] == false);
    CHECK(rubato["status"] == "unavailable");
    CHECK(response["availability"]["melodic"]["fields"]["ornament_density"]["available"] == false);
    const auto inspected = field_call(server, "get_work_analysis", {{"work_id", 2}});
    CHECK(inspected["availability"]["melodic"]["unavailable_fields"] ==
          json::array({"lanes_with_fewer_than_two_attacks.conjunct_proportion"}));
}

TEST_CASE("MCP field qualification preserves legacy economy without claiming computed accuracy",
          "[mcp][corpus][field-evidence]") {
    WorkAnalysis legacy;
    legacy.motivic_analysis.thematic_economy = 0.8f;
    WorkAnalysis automatic;
    automatic.evidence["motivic"] = {AnalysisEvidenceKind::Heuristic,
                                     "Finite repeated windows",
                                     std::nullopt,
                                     {"thematic_economy"},
                                     6};
    auto session = field_session({legacy, automatic});
    McpServer server;
    register_corpus_tools(server, session);
    const auto response = field_call(server, "query_style_profile", {{"composer_id", 1}});
    CHECK(response["profile"]["motivic_profile"]["thematic_economy"].get<float>() ==
          Catch::Approx(0.8f));
    const auto economy = response["availability"]["motivic"]["fields"]["thematic_economy"];
    CHECK(economy["computed_works"] == 0);
    CHECK(economy["unqualified_works"] == 1);
    CHECK(economy["unavailable_works"] == 1);
    CHECK(economy["available"] == true);
    CHECK(economy["status"] == "unqualified");
    CHECK(economy["fully_qualified"] == false);
}

TEST_CASE("MCP cross-domain field counts and methods follow actual source dependencies",
          "[mcp][corpus][field-evidence]") {
    WorkAnalysis analysis;
    analysis.dynamic_analysis.climax_position = 0.0f;
    analysis.evidence["dynamic"] = {
        AnalysisEvidenceKind::ExactSymbolic, "Measured initial climax", std::nullopt, {}, 1};
    analysis.evidence["formal"] = {
        AnalysisEvidenceKind::Unavailable, "No formal inventory", "No supplied sections", {}, 0};
    auto session = field_session({analysis});
    McpServer server;
    register_corpus_tools(server, session);
    const auto response = field_call(server, "query_style_profile", {{"composer_id", 1}});
    const auto climax = response["availability"]["formal"]["fields"]["climax_placement"];
    CHECK(climax["computed_works"] == 1);
    CHECK(climax["methods"] == json::array({"Measured initial climax"}));
    CHECK(climax["source_fields"] ==
          json::array({{{"domain", "dynamic"}, {"path", "climax_position"}}}));
    CHECK(response["availability"]["formal"]["computed_works"] == 0);
    // Zero is a valid observed climax at the beginning of the work.
    CHECK(response["profile"]["formal_profile"]["climax_placement"] == 0.0f);
}

TEST_CASE("MCP ingestion failures expose core codes without changing corpus counters",
          "[mcp][corpus][field-evidence][ingestion]") {
    auto session = field_session({});
    McpServer server;
    register_corpus_tools(server, session);
    const auto midi =
        field_call(server,
                   "ingest_midi",
                   {{"composer_id", 1}, {"title", "Broken MIDI"}, {"midi_base64", "AA=="}});
    CHECK(midi["error"] == "MIDI ingestion failed");
    CHECK(midi["error_code"] == static_cast<int>(ErrorCode::IngestionFailed));
    CHECK_FALSE(midi["message"].get<std::string>().empty());
    const auto xml = field_call(server,
                                "ingest_musicxml",
                                {{"composer_id", 1}, {"title", "Broken XML"}, {"musicxml", "<"}});
    CHECK(xml["error"] == "MusicXML ingestion failed");
    CHECK(xml["error_code"] == static_cast<int>(ErrorCode::IngestionFailed));
    CHECK_FALSE(xml["message"].get<std::string>().empty());
    CHECK(session->next_work_id == 1);
    CHECK(session->corpus.works.empty());
}

TEST_CASE("all corpus identity allocation tools reject stale and exhausted counters atomically",
          "[mcp][corpus][field-evidence][identity]") {
    struct Call {
        const char* tool;
        bool composer;
        json arguments;
    };
    const std::vector<Call> calls = {
        {"create_composer_profile", true, {{"name", "New composer"}}},
        {"create_ingested_work",
         false,
         {{"title", "New work"}, {"source_format", "MIDI"}, {"composer_id", 1}}},
        {"ingest_midi",
         false,
         {{"title", "New MIDI"}, {"composer_id", 1}, {"midi_base64", "AA=="}}},
        {"ingest_musicxml", false, {{"title", "New XML"}, {"composer_id", 1}, {"musicxml", "<"}}},
        {"ingest_batch",
         false,
         {{"works",
           {{{"title", "Batch MIDI"}, {"composer_id", 1}, {"format", "midi"}, {"data", "AA=="}}}}}},
        {"ingest_batch",
         false,
         {{"works",
           {{{"title", "Batch XML"},
             {"composer_id", 1},
             {"format", "musicxml"},
             {"musicxml", "<"}}}}}},
    };
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    for (const auto& call : calls) {
        for (const std::uint64_t next : {std::uint64_t{0}, std::uint64_t{1}, maximum}) {
            DYNAMIC_SECTION(call.tool << " next=" << next << " composer=" << call.composer) {
                auto session = field_session({});
                if (call.composer) {
                    session->corpus.composers[10] =
                        create_composer_profile(ComposerProfileId{10}, "Imported composer");
                    session->next_composer_id = next;
                } else {
                    IngestedWork imported;
                    imported.id = IngestedWorkId{10};
                    imported.metadata.title = "Imported work";
                    session->corpus.works[10] = imported;
                    session->next_work_id = next;
                }
                const auto before = corpus_to_json(session->corpus);
                McpServer server;
                register_corpus_tools(server, session);
                auto response = field_call(server, call.tool, call.arguments);
                if (response.contains("results")) response = response["results"][0];
                CHECK(response.contains("error"));
                CHECK(response["error_code"] ==
                      static_cast<int>(next == 1 ? ErrorCode::InvariantViolation
                                                 : ErrorCode::ArithmeticOverflow));
                CHECK(corpus_to_json(session->corpus) == before);
                CHECK((call.composer ? session->next_composer_id : session->next_work_id) == next);
            }
        }
    }
}

TEST_CASE("corpus global identities observe imported roots and allocate their finite last ID once",
          "[mcp][corpus][field-evidence][identity]") {
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    SECTION("composer last ID") {
        auto session = field_session({});
        session->next_composer_id = maximum - 1;
        McpServer server;
        register_corpus_tools(server, session);
        CHECK(field_call(server,
                         "create_composer_profile",
                         {{"name", "Last composer"}})["composer_id"] == maximum - 1);
        CHECK(session->next_composer_id == maximum);
        const auto before = corpus_to_json(session->corpus);
        CHECK(field_call(server, "create_composer_profile", {{"name", "Exhausted"}})
                  .contains("error"));
        CHECK(corpus_to_json(session->corpus) == before);
    }
    SECTION("work last ID") {
        auto session = field_session({});
        session->next_work_id = maximum - 1;
        McpServer server;
        register_corpus_tools(server, session);
        const json args = {{"title", "Last work"}, {"source_format", "MIDI"}, {"composer_id", 1}};
        CHECK(field_call(server, "create_ingested_work", args)["work_id"] == maximum - 1);
        CHECK(session->next_work_id == maximum);
        const auto before = corpus_to_json(session->corpus);
        CHECK(field_call(server, "create_ingested_work", args).contains("error"));
        CHECK(corpus_to_json(session->corpus) == before);
    }
    SECTION("observe mismatched embedded imported ID before allocating") {
        auto session = field_session({});
        IngestedWork imported;
        imported.id = IngestedWorkId{maximum};
        session->corpus.works[8] = imported;
        session->next_work_id = 9;
        McpServer server;
        register_corpus_tools(server, session);
        const auto before = corpus_to_json(session->corpus);
        const auto response = field_call(
            server, "create_ingested_work", {{"title", "No overwrite"}, {"source_format", "MIDI"}});
        CHECK(response["error_code"] == static_cast<int>(ErrorCode::ArithmeticOverflow));
        CHECK(session->next_work_id == 9);
        CHECK(corpus_to_json(session->corpus) == before);
    }
    SECTION("failed source parsing retains an otherwise valid allocation") {
        auto session = field_session({});
        IngestedWork imported;
        imported.id = IngestedWorkId{10};
        session->corpus.works[10] = imported;
        session->next_work_id = 11;
        McpServer server;
        register_corpus_tools(server, session);
        const auto before = corpus_to_json(session->corpus);
        CHECK(field_call(server,
                         "ingest_midi",
                         {{"composer_id", 1}, {"title", "Invalid"}, {"midi_base64", "AA=="}})
                  .contains("error"));
        CHECK(session->next_work_id == 11);
        CHECK(corpus_to_json(session->corpus) == before);
    }
}
