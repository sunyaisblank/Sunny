/** Corpus queries expose real records and distinguish missing analytical evidence. */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sunny/core/corpus/workflows.hpp>
#include <sunny/infrastructure/mcp/corpus_tools.hpp>

using namespace sunny::core;
using namespace sunny::infrastructure;
using json = nlohmann::json;

namespace {
json corpus_call(McpServer& server, const std::string& name, const json& args) {
    const auto reply = server.process_request({{"jsonrpc", "2.0"},
                                               {"id", 1},
                                               {"method", "tools/call"},
                                               {"params", {{"name", name}, {"arguments", args}}}});
    REQUIRE(reply.contains("result"));
    return json::parse(reply["result"]["content"][0]["text"].get<std::string>());
}
} // namespace

TEST_CASE("Corpus MCP inspection reports complete records and domain qualification",
          "[mcp][corpus][evidence]") {
    auto session = std::make_shared<CorpusSession>();
    session->corpus.composers[1] =
        create_composer_profile(ComposerProfileId{1}, "Observed composer");
    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.analysis_complete = true;
    work.analysis.harmonic_analysis.chord_vocabulary = {{"I", 4}, {"V", 2}};
    work.analysis.evidence["harmonic"] = {AnalysisEvidenceKind::Heuristic,
                                          "exact overlap with finite chord recognition",
                                          std::nullopt,
                                          {"enharmonic_intent"},
                                          6};
    work.analysis.evidence["motivic"] = {AnalysisEvidenceKind::Unavailable,
                                         "finite repeated monophonic windows",
                                         "No eligible windows",
                                         {},
                                         0};
    session->corpus.works[1] = work;
    REQUIRE(assign_work_to_composer(session->corpus, IngestedWorkId{1}, ComposerProfileId{1}));
    McpServer server;
    register_corpus_tools(server, session);

    const auto inspected = corpus_call(server, "get_work_analysis", {{"work_id", 1}});
    REQUIRE_FALSE(inspected.contains("error"));
    CHECK(inspected["analysis"]["harmonic_analysis"]["chord_vocabulary"]["I"] == 4);
    CHECK(inspected["analysis"]["evidence"]["harmonic"]["observations"] == 6);
    CHECK(inspected["availability"]["harmonic"]["available"] == true);
    CHECK(inspected["availability"]["motivic"]["status"] == "unavailable");
    CHECK(inspected["availability"]["melodic"]["status"] == "unqualified");
    const auto profile = corpus_call(server, "query_style_profile", {{"composer_id", 1}});
    CHECK(profile["profile"]["harmonic_profile"]["chord_frequency"]["I"].get<float>() ==
          Catch::Approx(4.0 / 6.0));
    CHECK(profile["availability"]["harmonic"]["computed_works"] == 1);
    CHECK(profile["availability"]["motivic"]["unavailable_works"] == 1);
    CHECK(profile["availability"]["melodic"]["unqualified_works"] == 1);
    CHECK(profile["confidence_interpretation"].get<std::string>().find("heuristic") !=
          std::string::npos);
    CHECK(corpus_call(server, "get_work_analysis", {{"work_id", 99}}).contains("error"));
}
