/**
 * @file server_test.cpp
 * @brief MCP JSON-RPC Server unit tests
 *
 *
 * Coverage: JSON-RPC parse, tool dispatch, error responses,
 *           protocol conformance
 *
 * Tests call the parsed-request seam used by the real run loop, avoiding
 * process-wide stdin/stdout redirection without duplicating dispatch logic.
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <future>
#include <latch>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/harmony/roman_numeral.hpp>
#include <sunny/core/scale/definitions.hpp>
#include <sunny/infrastructure/mcp/core_tools.hpp>
#include <sunny/infrastructure/mcp/corpus_tools.hpp>
#include <sunny/infrastructure/mcp/mix_tools.hpp>
#include <sunny/infrastructure/mcp/project_tools.hpp>
#include <sunny/infrastructure/mcp/score_tools.hpp>
#include <sunny/infrastructure/mcp/server.hpp>
#include <sunny/infrastructure/mcp/timbre_tools.hpp>
#include <vector>

using namespace sunny::infrastructure;
using json = nlohmann::json;
using namespace std::chrono_literals;

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

void check_diagnostic_contract(const json& diagnostic) {
    REQUIRE(diagnostic.is_object());
    CHECK(diagnostic["rule"].is_string());
    CHECK(diagnostic["severity"].is_string());
    CHECK(diagnostic["message"].is_string());
    CHECK(diagnostic["error_code"].is_number_integer());
}

TEST_CASE("Score MCP ties a note only to an adjacent same-pitch continuation",
          "[mcp][score][tie]") {
    McpServer server;
    auto session = std::make_shared<ScoreSession>();
    register_score_tools(server, nullptr, session);
    const auto created = call_tool(server,
                                   "score_create",
                                   {{"total_bars", 2},
                                    {"time_sig_num", 4},
                                    {"time_sig_den", 4},
                                    {"parts", {{{"name", "Line"}, {"instrument_type", 0}}}}},
                                   3101);
    const json g4 = {{"letter", "G"}, {"accidental", 0}, {"octave", 4}};
    const json a4 = {{"letter", "A"}, {"accidental", 0}, {"octave", 4}};
    for (const auto& [bar, offset, pitch] : {std::tuple{1, json{{"n", 3}, {"d", 4}}, g4},
                                             std::tuple{2, json{{"n", 0}, {"d", 1}}, g4},
                                             std::tuple{2, json{{"n", 1}, {"d", 4}}, a4}}) {
        REQUIRE(call_tool(server,
                          "score_insert_note",
                          {{"score_id", created["score_id"]},
                           {"part_id", created["part_ids"][0]},
                           {"bar", bar},
                           {"offset", offset},
                           {"pitch", pitch},
                           {"duration", {{"n", 1}, {"d", 4}}}},
                          3102)["ok"] == true);
    }
    const auto note_event = [&](int bar, int num, int den) {
        const auto document =
            call_tool(server, "score_get_json", {{"score_id", created["score_id"]}}, 3103);
        for (const auto& event : document["parts"][0]["measures"][bar - 1]["voices"][0]["events"])
            if (event["type"] == "note_group" && event["offset"]["num"] == num &&
                event["offset"]["den"] == den)
                return event;
        FAIL("note event not found");
        return json{};
    };

    const auto across_bar = note_event(1, 3, 4);
    CHECK(call_tool(
              server,
              "score_set_tie",
              {{"score_id", created["score_id"]}, {"event_id", across_bar["id"]}, {"tied", true}},
              3104)["ok"] == true);
    CHECK(note_event(1, 3, 4)["notes"][0]["tie_forward"] == true);

    const auto to_other_pitch = note_event(2, 0, 1);
    CHECK(
        call_tool(
            server,
            "score_set_tie",
            {{"score_id", created["score_id"]}, {"event_id", to_other_pitch["id"]}, {"tied", true}},
            3105)
            .contains("error"));
    CHECK_FALSE(note_event(2, 0, 1)["notes"][0].contains("tie_forward"));
}

TEST_CASE("Score MCP handle and serialized root identity remain identical",
          "[mcp][score][identity][serialization]") {
    McpServer server;
    auto session = std::make_shared<ScoreSession>();
    register_score_tools(server, nullptr, session);
    const json create_params = {{"title", "Identity"},
                                {"total_bars", 1},
                                {"parts", {{{"name", "Piano"}, {"instrument_type", 0}}}}};

    const auto created = call_tool(server, "score_create", create_params, 3001);
    REQUIRE(created["score_id"] == 1);
    const auto created_json =
        call_tool(server, "score_get_json", {{"score_id", created["score_id"]}}, 3002);
    CHECK(created_json["id"] == created["score_id"]);

    const auto reduction =
        call_tool(server, "score_get_reduction", {{"score_id", created["score_id"]}}, 3003);
    REQUIRE(reduction["score_id"] == 2);
    const auto reduction_json =
        call_tool(server, "score_get_json", {{"score_id", reduction["score_id"]}}, 3004);
    CHECK(reduction_json["id"] == reduction["score_id"]);

    const auto second = call_tool(server, "score_create", create_params, 3005);
    REQUIRE(second["score_id"] == 3);
    REQUIRE(session->find(3) != nullptr);
    CHECK(session->find(3)->id == sunny::core::ScoreId{3});
}

TEST_CASE("Score MCP identity exhaustion never wraps or overwrites",
          "[mcp][score][identity][atomicity]") {
    McpServer server;
    auto session = std::make_shared<ScoreSession>();
    session->next_score_id = std::numeric_limits<std::uint64_t>::max();
    register_score_tools(server, nullptr, session);
    const json create_params = {{"title", "Last identity"},
                                {"total_bars", 1},
                                {"parts", {{{"name", "Piano"}, {"instrument_type", 0}}}}};

    const auto last = call_tool(server, "score_create", create_params, 3010);
    REQUIRE(last["score_id"] == std::numeric_limits<std::uint64_t>::max());
    REQUIRE(session->scores.size() == 1);
    REQUIRE(session->find(std::numeric_limits<std::uint64_t>::max()) != nullptr);
    CHECK(session->find(std::numeric_limits<std::uint64_t>::max())->id ==
          sunny::core::ScoreId{std::numeric_limits<std::uint64_t>::max()});

    const auto rejected_create = call_tool(server, "score_create", create_params, 3011);
    CHECK(rejected_create["error"] == "score identity domain exhausted");
    const auto rejected_reduction =
        call_tool(server,
                  "score_get_reduction",
                  {{"score_id", std::numeric_limits<std::uint64_t>::max()}},
                  3012);
    CHECK(rejected_reduction["error"] == "score identity domain exhausted");
    CHECK(session->scores.size() == 1);
    CHECK(session->next_score_id == std::numeric_limits<std::uint64_t>::max());
}

TEST_CASE("Score MCP authors one complete tuning atomically and exposes target residuals",
          "[mcp][score][tuning]") {
    McpServer server;
    register_score_tools(server);
    const auto listed =
        server.process_request({{"jsonrpc", "2.0"}, {"method", "tools/list"}, {"id", 3019}});
    const auto& tools = listed["result"]["tools"];
    const auto create_tool = std::find_if(tools.begin(), tools.end(), [](const auto& tool) {
        return tool["name"] == "score_create";
    });
    const auto set_tool = std::find_if(tools.begin(), tools.end(), [](const auto& tool) {
        return tool["name"] == "score_set_tuning";
    });
    REQUIRE(create_tool != tools.end());
    REQUIRE(set_tool != tools.end());
    CHECK((*create_tool)["inputSchema"]["properties"]["tuning"]["type"] == "object");
    CHECK((*set_tool)["inputSchema"]["properties"]["reference_frequency_hz"]["type"] == "number");
    CHECK((*set_tool)["inputSchema"]["properties"]["cents_from_reference"]["type"] == "array");

    json cents = json::array();
    for (int note = 0; note < 128; ++note)
        cents.push_back((note - 69) * 100.0);
    cents[60] = -900.75;
    const auto created = call_tool(server,
                                   "score_create",
                                   {{"title", "Tuned"},
                                    {"total_bars", 1},
                                    {"parts", {{{"name", "Instrument"}, {"instrument_type", 80}}}},
                                    {"tuning",
                                     {{"name", "creation tuning"},
                                      {"reference_midi_note", 69},
                                      {"reference_frequency_hz", 440.0},
                                      {"cents_from_reference", cents}}}},
                                   3020);

    const auto initial =
        call_tool(server, "score_get_json", {{"score_id", created["score_id"]}}, 30201);
    CHECK(initial["tuning"]["name"] == "creation tuning");
    CHECK(initial["tuning"]["cents_from_reference"][60] == -900.75);

    cents[60] = -901.25;
    const auto changed = call_tool(server,
                                   "score_set_tuning",
                                   {{"score_id", created["score_id"]},
                                    {"name", "MCP custom"},
                                    {"reference_midi_note", 69},
                                    {"reference_frequency_hz", 440.0},
                                    {"cents_from_reference", cents}},
                                   3021);
    REQUIRE(changed["ok"] == true);
    const auto stored =
        call_tool(server, "score_get_json", {{"score_id", created["score_id"]}}, 3022);
    CHECK(stored["schema_version"] == 8);
    CHECK(stored["tuning"]["name"] == "MCP custom");
    CHECK(stored["tuning"]["cents_from_reference"][60] == -901.25);

    cents.erase(cents.begin());
    const auto rejected = call_tool(server,
                                    "score_set_tuning",
                                    {{"score_id", created["score_id"]},
                                     {"name", "Incomplete"},
                                     {"reference_midi_note", 69},
                                     {"reference_frequency_hz", 440.0},
                                     {"cents_from_reference", cents}},
                                    3023);
    CHECK(rejected.contains("error"));
    const auto after =
        call_tool(server, "score_get_json", {{"score_id", created["score_id"]}}, 3024);
    CHECK(after["tuning"] == stored["tuning"]);
    CHECK(after["version"] == stored["version"]);

    const auto midi =
        call_tool(server, "score_compile_to_midi", {{"score_id", created["score_id"]}}, 3025);
    REQUIRE(midi["time_signatures"].size() == 1);
    CHECK(midi["time_signatures"][0]["clocks_per_metronome_click"] == 24);
    CHECK(midi["time_signatures"][0]["notated_32nds_per_quarter"] == 8);
    CHECK(midi["report"]["time_signature_events_requested"] == 1);
    CHECK(midi["report"]["time_signature_events_written"] == 1);
    CHECK(midi["report"]["time_signature_groupings_requested"] == 0);
    CHECK(midi["report"]["time_signature_groupings_written"] == 0);
    CHECK(midi["report"]["tuning_definitions_requested"] == 1);
    CHECK(midi["report"]["tuning_definitions_written"] == 0);
}

} // namespace

class TestMcpServer : public McpServer {
  public:
    json dispatch(const json& request) { return process_request(request); }

    void add_tool(const std::string& name, McpToolHandler handler) {
        register_tool(name, "test tool", json::object(), std::move(handler));
    }
};

class SessionTransport final : public LomTransport {
  public:
    LomResponse send(const LomRequest& request) override {
        // The Remote Script declines anything outside the closed bridge algebra.
        if (!LomProtocol::validate_request(request))
            return {false, std::nullopt, "outside the bridge protocol"};
        if (request.property_or_method == "tempo") return ok(128.0);
        if (request.property_or_method == "signature_numerator") return ok(7);
        if (request.property_or_method == "signature_denominator") return ok(8);
        if (request.property_or_method == "is_playing") return ok(true);
        if (request.property_or_method == "current_song_time") return ok(12.5);
        if (request.property_or_method == "sunny_get_track_count") return ok(3);
        if (request.property_or_method == "sunny_get_return_track_count") return ok(2);
        return {false, std::nullopt, "unexpected property"};
    }

    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override {
        return {true, std::nullopt, std::nullopt};
    }

    [[nodiscard]] bool is_connected() const override { return true; }

    [[nodiscard]] sunny::core::Result<std::optional<AbletonTargetProfile>>
    target_profile() override {
        return std::optional<AbletonTargetProfile>{modeled_target_profile({12, 3, 5, "12.3.5"})};
    }

    [[nodiscard]] sunny::core::Result<std::optional<std::uint32_t>> scene_count() override {
        return std::optional<std::uint32_t>{1};
    }

  private:
    static LomResponse ok(LomValue value) { return {true, std::move(value), std::nullopt}; }
};

// =============================================================================
// Protocol Tests
// =============================================================================

TEST_CASE("initialize response", "[mcp][protocol]") {
    TestMcpServer server;

    auto resp = server.dispatch({{"jsonrpc", "2.0"},
                                 {"method", "initialize"},
                                 {"params", {{"protocolVersion", "2025-11-25"}}},
                                 {"id", 1}});

    REQUIRE(resp.contains("result"));
    auto& result = resp["result"];
    CHECK(result["protocolVersion"] == "2025-11-25");
    CHECK(result["serverInfo"]["name"] == "sunny-mcp");
    CHECK(result["serverInfo"]["version"] == "0.4.0");
    CHECK(result["capabilities"]["tools"]["listChanged"] == false);
    CHECK(resp["id"] == 1);
}

TEST_CASE("legacy initialize negotiates supported and fallback versions", "[mcp][protocol]") {
    TestMcpServer server;

    auto compatible = server.dispatch({{"jsonrpc", "2.0"},
                                       {"method", "initialize"},
                                       {"params", {{"protocolVersion", "2024-11-05"}}},
                                       {"id", 10}});
    CHECK(compatible["result"]["protocolVersion"] == "2024-11-05");

    auto fallback = server.dispatch({{"jsonrpc", "2.0"},
                                     {"method", "initialize"},
                                     {"params", {{"protocolVersion", "unsupported"}}},
                                     {"id", 11}});
    CHECK(fallback["result"]["protocolVersion"] == "2025-11-25");

    auto missing =
        server.dispatch({{"jsonrpc", "2.0"}, {"method", "initialize"}, {"params", {}}, {"id", 12}});
    CHECK(missing["error"]["code"] == -32602);
}

TEST_CASE("modern discovery advertises stateless MCP support", "[mcp][protocol]") {
    TestMcpServer server;
    auto response = server.dispatch(
        {{"jsonrpc", "2.0"},
         {"method", "server/discover"},
         {"params",
          {{"_meta",
            {{"io.modelcontextprotocol/protocolVersion", "2026-07-28"},
             {"io.modelcontextprotocol/clientInfo", {{"name", "test"}, {"version", "1"}}},
             {"io.modelcontextprotocol/clientCapabilities", json::object()}}}}},
         {"id", "discover"}});

    REQUIRE(response.contains("result"));
    const auto& result = response["result"];
    CHECK(result["resultType"] == "complete");
    CHECK(result["supportedVersions"][0] == "2026-07-28");
    CHECK(result["capabilities"]["tools"]["listChanged"] == false);
    CHECK(result["_meta"]["io.modelcontextprotocol/serverInfo"]["name"] == "sunny-mcp");
}

TEST_CASE("missing jsonrpc field", "[mcp][protocol]") {
    TestMcpServer server;

    auto resp = server.dispatch({{"method", "initialize"}, {"id", 1}});

    REQUIRE(resp.contains("error"));
    CHECK(resp["error"]["code"] == -32600);
}

TEST_CASE("wrong jsonrpc version", "[mcp][protocol]") {
    TestMcpServer server;

    auto resp = server.dispatch({{"jsonrpc", "1.0"}, {"method", "initialize"}, {"id", 1}});

    REQUIRE(resp.contains("error"));
    CHECK(resp["error"]["code"] == -32600);
}

TEST_CASE("missing method", "[mcp][protocol]") {
    TestMcpServer server;

    auto resp = server.dispatch({{"jsonrpc", "2.0"}, {"id", 1}});

    REQUIRE(resp.contains("error"));
    CHECK(resp["error"]["code"] == -32600);
}

TEST_CASE("unknown method", "[mcp][protocol]") {
    TestMcpServer server;

    auto resp = server.dispatch({{"jsonrpc", "2.0"}, {"method", "unknown/method"}, {"id", 1}});

    REQUIRE(resp.contains("error"));
    CHECK(resp["error"]["code"] == -32601);
}

TEST_CASE("notification returns null", "[mcp][protocol]") {
    TestMcpServer server;

    auto resp = server.dispatch({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});

    CHECK(resp.is_null());
}

// =============================================================================
// Stdio loop conformance (JSON-RPC 2.0 sections 4-6)
// =============================================================================

namespace {

/// Drive the real newline-delimited run loop and return every line it wrote.
std::vector<json> run_stdio_session(McpServer& server, const std::vector<std::string>& lines) {
    std::string input;
    for (const auto& line : lines)
        input += line + "\n";
    std::istringstream in(input);
    std::ostringstream out;
    server.run(in, out);

    std::vector<json> responses;
    std::istringstream written(out.str());
    std::string line;
    while (std::getline(written, line)) {
        INFO("server wrote: " << line);
        responses.push_back(json::parse(line));
    }
    return responses;
}

} // namespace

TEST_CASE("stdio loop answers each non-object message with -32600 and keeps serving",
          "[mcp][protocol][stdio]") {
    TestMcpServer server;
    server.add_tool("echo", [](const json& params) -> json { return params; });
    const std::string tools_list = R"({"jsonrpc":"2.0","method":"tools/list","id":)";

    const auto responses = run_stdio_session(server,
                                             {"[]",
                                              tools_list + "1}",
                                              R"("just a string")",
                                              tools_list + "2}",
                                              "5",
                                              tools_list + "3}",
                                              "null",
                                              tools_list + "4}",
                                              "[1,2]",
                                              tools_list + "5}",
                                              "{not json",
                                              tools_list + "6}"});

    REQUIRE(responses.size() == 12);
    for (std::size_t index = 0; index < responses.size(); index += 2) {
        CAPTURE(index);
        const auto& listed = responses[index + 1];
        // "[1,2]" is a batch of two invalid requests: JSON-RPC 2.0 section 6
        // answers each element, so it yields an array of two errors.
        const auto errors = index == 8 ? responses[index] : json::array({responses[index]});
        CHECK(errors.size() == (index == 8 ? 2U : 1U));
        for (const auto& error : errors) {
            CHECK(error["jsonrpc"] == "2.0");
            CHECK(error["id"].is_null());
            CHECK(error["error"]["code"] == (index == 10 ? -32700 : -32600));
        }
        CHECK(listed["id"] == static_cast<int>(index / 2 + 1));
        CHECK(listed["result"]["tools"].size() == 1);
    }
}

TEST_CASE("stdio loop answers a batch element by element and omits notifications",
          "[mcp][protocol][stdio]") {
    // JSON-RPC 2.0 section 6; MCP 2025-03-26, which initialize negotiates,
    // requires servers to accept batches.
    TestMcpServer server;
    server.add_tool("echo", [](const json& params) -> json { return params; });

    const auto responses =
        run_stdio_session(server,
                          {R"([{"jsonrpc":"2.0","method":"ping","id":"a"},)"
                           R"({"jsonrpc":"2.0","method":"notifications/initialized"},)"
                           R"({"jsonrpc":"2.0","method":"tools/list","id":7},)"
                           R"(42])",
                           R"([{"jsonrpc":"2.0","method":"notifications/initialized"}])",
                           R"({"jsonrpc":"2.0","method":"ping","id":8})"});

    // The all-notification batch produces no output at all.
    REQUIRE(responses.size() == 2);
    const auto& batch = responses[0];
    REQUIRE(batch.is_array());
    REQUIRE(batch.size() == 3);
    CHECK(batch[0]["id"] == "a");
    CHECK(batch[0]["result"] == json::object());
    CHECK(batch[1]["id"] == 7);
    CHECK(batch[1]["result"]["tools"].size() == 1);
    CHECK(batch[2]["id"].is_null());
    CHECK(batch[2]["error"]["code"] == -32600);
    CHECK(responses[1]["id"] == 8);
}

TEST_CASE("stdio loop answers ping with an empty result", "[mcp][protocol][stdio]") {
    TestMcpServer server;

    const auto responses = run_stdio_session(server,
                                             {R"({"jsonrpc":"2.0","method":"ping","id":"p-1"})",
                                              R"({"jsonrpc":"2.0","method":"ping","id":2})"});

    REQUIRE(responses.size() == 2);
    CHECK(responses[0]["id"] == "p-1");
    CHECK(responses[0]["result"] == json::object());
    CHECK_FALSE(responses[0].contains("error"));
    CHECK(responses[1]["id"] == 2);
    CHECK(responses[1]["result"] == json::object());
}

TEST_CASE("stdio loop never answers notifications or client responses", "[mcp][protocol][stdio]") {
    TestMcpServer server;

    const auto responses =
        run_stdio_session(server,
                          {R"({"jsonrpc":"2.0","method":"notifications/initialized"})",
                           R"({"jsonrpc":"2.0","method":"notifications/initialized","params":[1]})",
                           R"({"jsonrpc":"2.0","method":"notifications/cancelled","params":"x"})",
                           R"({"jsonrpc":"2.0","method":"tools/list"})",
                           R"({"jsonrpc":"2.0","method":"unknown/method","params":7})",
                           R"({"jsonrpc":"2.0","id":5,"result":{}})",
                           R"({"jsonrpc":"2.0","id":"s-6","error":{"code":-1,"message":"no"}})",
                           R"({"jsonrpc":"2.0","method":"ping","id":9})"});

    REQUIRE(responses.size() == 1);
    CHECK(responses[0]["id"] == 9);
    CHECK(responses[0]["result"] == json::object());
}

TEST_CASE("stdio loop rejects structurally invalid request objects with a null id",
          "[mcp][protocol][stdio]") {
    TestMcpServer server;

    const auto responses = run_stdio_session(server,
                                             {R"({"jsonrpc":"2.0"})",
                                              R"({"method":"ping","id":{"nested":true}})",
                                              R"({"jsonrpc":"2.0","method":"ping","id":1.5})",
                                              R"({"jsonrpc":"2.0","method":"ping","id":null})",
                                              R"({"jsonrpc":"2.0","method":"ping","id":3})"});

    REQUIRE(responses.size() == 5);
    for (std::size_t index = 0; index < 4; ++index) {
        CAPTURE(index);
        CHECK(responses[index]["id"].is_null());
        CHECK(responses[index]["error"]["code"] == -32600);
    }
    CHECK(responses[4]["id"] == 3);
    CHECK(responses[4]["result"] == json::object());
}

TEST_CASE("integer schemas accept numbers with a zero fractional part", "[mcp][tools][schema]") {
    TestMcpServer server;
    server.register_tool("count",
                         "count tool",
                         {{"type", "object"},
                          {"properties",
                           {{"count", {{"type", "integer"}}},
                            {"values", {{"type", "array"}, {"items", {{"type", "integer"}}}}}}},
                          {"required", json::array({"count"})}},
                         [](const json& params) -> json {
                             int total = sunny::core::detail::checked_integer<int>(
                                 params.at("count"), "count");
                             for (const auto& value : params.value("values", json::array()))
                                 total += sunny::core::detail::checked_integer<int>(value, "value");
                             return {{"total", total}};
                         });

    const auto responses = run_stdio_session(
        server,
        {R"({"jsonrpc":"2.0","method":"tools/call","id":1,"params":{"name":"count","arguments":{"count":4.0}}})",
         R"({"jsonrpc":"2.0","method":"tools/call","id":2,"params":{"name":"count","arguments":{"count":1e2,"values":[2.0,-3]}}})",
         R"({"jsonrpc":"2.0","method":"tools/call","id":3,"params":{"name":"count","arguments":{"count":4.5}}})",
         R"({"jsonrpc":"2.0","method":"tools/call","id":4,"params":{"name":"count","arguments":{"count":1,"values":[0.25]}}})"});

    REQUIRE(responses.size() == 4);
    CHECK(responses[0]["result"]["isError"] == false);
    CHECK(responses[0]["result"]["structuredContent"]["total"] == 4);
    CHECK(responses[1]["result"]["isError"] == false);
    CHECK(responses[1]["result"]["structuredContent"]["total"] == 99);
    CHECK(responses[2]["result"]["isError"] == true);
    CHECK(responses[2]["result"]["structuredContent"]["error"] ==
          "arguments.count must be of type integer");
    CHECK(responses[3]["result"]["isError"] == true);
    CHECK(responses[3]["result"]["structuredContent"]["error"] ==
          "arguments.values[0] must be of type integer");
}

TEST_CASE("parsed MCP requests serialize handler execution", "[mcp][concurrency]") {
    TestMcpServer server;
    std::latch first_entered{1};
    std::latch release_first{1};
    std::latch second_attempted{1};
    std::atomic_bool second_entered{false};

    server.add_tool("serialize", [&](const json& params) -> json {
        if (params.value("first", false)) {
            first_entered.count_down();
            release_first.wait();
        } else {
            second_entered.store(true, std::memory_order_release);
        }
        return {{"ok", true}};
    });

    auto request = [](int id, bool first) {
        return json{{"jsonrpc", "2.0"},
                    {"method", "tools/call"},
                    {"params", {{"name", "serialize"}, {"arguments", {{"first", first}}}}},
                    {"id", id}};
    };
    auto first = std::async(std::launch::async, [&] { return server.dispatch(request(1, true)); });
    first_entered.wait();
    auto second = std::async(std::launch::async, [&] {
        second_attempted.count_down();
        return server.dispatch(request(2, false));
    });
    second_attempted.wait();

    const auto second_status = second.wait_for(250ms);
    const bool overlapped = second_entered.load(std::memory_order_acquire);
    release_first.count_down();
    const auto first_response = first.get();
    const auto second_response = second.get();

    CHECK(second_status == std::future_status::timeout);
    CHECK_FALSE(overlapped);
    CHECK(first_response["id"] == 1);
    CHECK(second_response["id"] == 2);
    CHECK(second_entered.load(std::memory_order_acquire));
}

// =============================================================================
// Tool Dispatch Tests
// =============================================================================

TEST_CASE("tools/list with no tools", "[mcp][tools]") {
    TestMcpServer server;

    auto resp = server.dispatch({{"jsonrpc", "2.0"}, {"method", "tools/list"}, {"id", 2}});

    REQUIRE(resp.contains("result"));
    auto& tools = resp["result"]["tools"];
    CHECK(tools.is_array());
    CHECK(tools.size() == 0);
}

TEST_CASE("tools/list with registered tools", "[mcp][tools]") {
    TestMcpServer server;

    server.add_tool("echo", [](const json& params) -> json { return params; });
    server.add_tool("add", [](const json& params) -> json {
        return params["a"].get<int>() + params["b"].get<int>();
    });

    auto resp = server.dispatch({{"jsonrpc", "2.0"}, {"method", "tools/list"}, {"id", 3}});

    REQUIRE(resp.contains("result"));
    auto& tools = resp["result"]["tools"];
    CHECK(tools.size() == 2);
}

TEST_CASE("modern tools responses carry result type, metadata, and structured content",
          "[mcp][protocol][tools]") {
    TestMcpServer server;
    server.add_tool("echo", [](const json& params) -> json { return params; });
    const json meta = {{"io.modelcontextprotocol/protocolVersion", "2026-07-28"},
                       {"io.modelcontextprotocol/clientInfo", {{"name", "test"}, {"version", "1"}}},
                       {"io.modelcontextprotocol/clientCapabilities", json::object()}};

    auto listed = server.dispatch(
        {{"jsonrpc", "2.0"}, {"method", "tools/list"}, {"params", {{"_meta", meta}}}, {"id", 20}});
    CHECK(listed["result"]["resultType"] == "complete");
    CHECK(listed["result"]["cacheScope"] == "public");
    CHECK(listed["result"]["_meta"]["io.modelcontextprotocol/serverInfo"]["name"] == "sunny-mcp");

    auto called = server.dispatch(
        {{"jsonrpc", "2.0"},
         {"method", "tools/call"},
         {"params", {{"name", "echo"}, {"arguments", {{"message", "hello"}}}, {"_meta", meta}}},
         {"id", 21}});
    CHECK(called["result"]["resultType"] == "complete");
    CHECK(called["result"]["isError"] == false);
    CHECK(called["result"]["structuredContent"]["message"] == "hello");
}

TEST_CASE("all public tools advertise object-shaped JSON Schemas", "[mcp][tools][schema]") {
    Orchestrator orchestrator;
    CommandBuffer transport;
    BridgeDispatcher dispatcher(&transport);
    McpServer server;
    McpSession session;
    register_sunny_tools(server, orchestrator, dispatcher);
    register_score_tools(server, &transport, session.score);
    register_timbre_tools(server, &transport, session.timbre);
    register_mix_tools(server, &transport, session.mix);
    register_corpus_tools(server, session.corpus);
    register_project_tools(server, session, &transport);

    auto response =
        server.process_request({{"jsonrpc", "2.0"}, {"method", "tools/list"}, {"id", 30}});
    const auto& tools = response["result"]["tools"];
    REQUIRE(tools.size() == 119);
    for (const auto& tool : tools) {
        CAPTURE(tool["name"]);
        const auto& schema = tool["inputSchema"];
        REQUIRE(schema.is_object());
        CHECK(schema["type"] == "object");
        REQUIRE(schema["properties"].is_object());
        REQUIRE(schema["required"].is_array());
        for (const auto& required_name : schema["required"]) {
            CHECK(schema["properties"].contains(required_name.get<std::string>()));
        }
    }
}

TEST_CASE("tools/call success", "[mcp][tools]") {
    TestMcpServer server;

    server.add_tool("add", [](const json& params) -> json {
        return params["a"].get<int>() + params["b"].get<int>();
    });

    auto resp = server.dispatch({{"jsonrpc", "2.0"},
                                 {"method", "tools/call"},
                                 {"params", {{"name", "add"}, {"arguments", {{"a", 3}, {"b", 4}}}}},
                                 {"id", 4}});

    REQUIRE(resp.contains("result"));
    auto& content = resp["result"]["content"];
    REQUIRE(content.is_array());
    REQUIRE(content.size() == 1);
    CHECK(content[0]["type"] == "text");
    CHECK(content[0]["text"] == "7");
    CHECK_FALSE(resp["result"].contains("structuredContent"));
    CHECK(resp["result"]["isError"] == false);
}

TEST_CASE("modern request metadata and versions are validated", "[mcp][protocol]") {
    TestMcpServer server;

    auto missing_capabilities = server.dispatch(
        {{"jsonrpc", "2.0"},
         {"method", "tools/list"},
         {"params", {{"_meta", {{"io.modelcontextprotocol/protocolVersion", "2026-07-28"}}}}},
         {"id", 24}});
    CHECK(missing_capabilities["error"]["code"] == -32602);

    auto unsupported =
        server.dispatch({{"jsonrpc", "2.0"},
                         {"method", "tools/list"},
                         {"params",
                          {{"_meta",
                            {{"io.modelcontextprotocol/protocolVersion", "2099-01-01"},
                             {"io.modelcontextprotocol/clientCapabilities", json::object()}}}}},
                         {"id", 25}});
    CHECK(unsupported["error"]["code"] == -32022);
    CHECK(unsupported["error"]["data"]["supported"][0] == "2026-07-28");
}

TEST_CASE("schema and business failures are visible tool execution errors", "[mcp][tools]") {
    TestMcpServer server;
    int invocations = 0;
    server.register_tool("validated",
                         "validated tool",
                         {{"type", "object"},
                          {"properties", {{"count", {{"type", "integer"}}}}},
                          {"required", json::array({"count"})}},
                         [&invocations](const json&) -> json {
                             ++invocations;
                             return {{"error", "business rule failed"}};
                         });

    auto invalid = server.dispatch(
        {{"jsonrpc", "2.0"},
         {"method", "tools/call"},
         {"params", {{"name", "validated"}, {"arguments", {{"count", "not-an-integer"}}}}},
         {"id", 22}});
    CHECK(invalid["result"]["isError"] == true);
    CHECK(invalid["result"]["structuredContent"]["error"] ==
          "arguments.count must be of type integer");
    CHECK(invocations == 0);

    auto declined =
        server.dispatch({{"jsonrpc", "2.0"},
                         {"method", "tools/call"},
                         {"params", {{"name", "validated"}, {"arguments", {{"count", 1}}}}},
                         {"id", 23}});
    CHECK(declined["result"]["isError"] == true);
    CHECK(declined["result"]["structuredContent"]["error"] == "business rule failed");
    CHECK(invocations == 1);
}

TEST_CASE("tool registration rejects duplicate and malformed definitions", "[mcp][tools]") {
    TestMcpServer server;
    server.add_tool("unique", [](const json&) -> json { return json::object(); });
    CHECK_THROWS_AS(server.add_tool("unique", [](const json&) -> json { return json::object(); }),
                    std::invalid_argument);
    CHECK_THROWS_AS(
        server.add_tool("contains spaces", [](const json&) -> json { return json::object(); }),
        std::invalid_argument);
    CHECK_THROWS_AS(server.register_tool("bad_schema",
                                         "bad",
                                         {{"type", "array"}},
                                         [](const json&) -> json { return json::object(); }),
                    std::invalid_argument);
}

TEST_CASE("tools/call unknown tool", "[mcp][tools]") {
    TestMcpServer server;

    auto resp = server.dispatch({{"jsonrpc", "2.0"},
                                 {"method", "tools/call"},
                                 {"params", {{"name", "nonexistent"}}},
                                 {"id", 5}});

    REQUIRE(resp.contains("error"));
    CHECK(resp["error"]["code"] == -32602);
}

TEST_CASE("tools/call missing name", "[mcp][tools]") {
    TestMcpServer server;

    auto resp =
        server.dispatch({{"jsonrpc", "2.0"}, {"method", "tools/call"}, {"params", {}}, {"id", 6}});

    REQUIRE(resp.contains("error"));
    CHECK(resp["error"]["code"] == -32602);
}

TEST_CASE("response preserves request id", "[mcp][protocol]") {
    TestMcpServer server;

    SECTION("integer id") {
        auto resp = server.dispatch({{"jsonrpc", "2.0"},
                                     {"method", "initialize"},
                                     {"params", {{"protocolVersion", "2025-11-25"}}},
                                     {"id", 42}});
        CHECK(resp["id"] == 42);
    }

    SECTION("string id") {
        auto resp = server.dispatch({{"jsonrpc", "2.0"},
                                     {"method", "initialize"},
                                     {"params", {{"protocolVersion", "2025-11-25"}}},
                                     {"id", "request-abc"}});
        CHECK(resp["id"] == "request-abc");
    }
}

TEST_CASE("real MCP progression call reaches note transport", "[mcp][integration][bridge]") {
    Orchestrator orchestrator;
    CommandBuffer transport;
    BridgeDispatcher dispatcher(&transport);
    McpServer server;
    register_sunny_tools(server, orchestrator, dispatcher);

    auto response = server.process_request({{"jsonrpc", "2.0"},
                                            {"method", "tools/call"},
                                            {"params",
                                             {{"name", "create_progression_clip"},
                                              {"arguments",
                                               {{"track_index", 0},
                                                {"slot_index", 1},
                                                {"root", "C"},
                                                {"scale", "major"},
                                                {"numerals", {"I", "IV", "V", "I"}}}}}},
                                            {"id", 41}});

    REQUIRE(response.contains("result"));
    auto result = json::parse(response["result"]["content"][0]["text"].get<std::string>());
    CHECK(result["success"] == true);
    CHECK(result["commands_sent"] == 2);
    REQUIRE(transport.entries().size() == 2);
    CHECK(transport.entries()[0].request.property_or_method == "create_clip");
    CHECK(transport.entries()[1].request.property_or_method == "add_new_notes");
    CHECK_FALSE(transport.entries()[1].notes.empty());

    auto undo_response = server.process_request({{"jsonrpc", "2.0"},
                                                 {"method", "tools/call"},
                                                 {"params", {{"name", "undo_ableton_operation"}}},
                                                 {"id", 42}});
    auto undo_result =
        json::parse(undo_response["result"]["content"][0]["text"].get<std::string>());
    CHECK(undo_result["success"] == true);
    REQUIRE(transport.entries().size() == 3);
    CHECK(transport.entries()[2].request.property_or_method == "delete_clip");

    auto redo_response = server.process_request({{"jsonrpc", "2.0"},
                                                 {"method", "tools/call"},
                                                 {"params", {{"name", "redo_ableton_operation"}}},
                                                 {"id", 43}});
    auto redo_result =
        json::parse(redo_response["result"]["content"][0]["text"].get<std::string>());
    CHECK(redo_result["success"] == true);
    REQUIRE(transport.entries().size() == 5);
    CHECK(transport.entries()[3].request.property_or_method == "create_clip");
    CHECK(transport.entries()[4].request.property_or_method == "add_new_notes");
}

namespace {

/**
 * Models Live's Session clip slots closely enough to observe what Sunny's
 * operations change. create_clip succeeds only on an empty slot (LOM
 * ClipSlot.create_clip), add_new_notes appends to an existing clip, and
 * delete_clip succeeds only on an occupied slot. Selected methods fail on
 * demand to model transient bridge failures.
 */
class ClipSlotModelTransport final : public LomTransport {
  public:
    std::map<std::string, std::vector<int>> clips; ///< slot path -> clip pitches
    std::map<std::string, int> failures;           ///< method -> upcoming calls to fail
    LomDeliveryState failure_delivery = LomDeliveryState::ResponseReceived;
    std::vector<std::string> methods; ///< every method that reached the model

    LomResponse send(const LomRequest& request) override {
        const auto& method = request.property_or_method;
        const auto slot = request.path.to_string();
        methods.push_back(method);
        if (inject_failure(method)) return injected(method);
        if (method == "create_clip") {
            if (clips.contains(slot)) return rejected("clip slot already has a clip");
            clips[slot] = {};
            return {true, std::nullopt, std::nullopt};
        }
        if (method == "delete_clip") {
            if (!clips.contains(slot)) return rejected("clip slot is empty");
            clips.erase(slot);
            return {true, std::nullopt, std::nullopt};
        }
        return rejected("unexpected request " + method);
    }

    LomResponse send_notes(const LomPath& clip_path,
                           const std::vector<LomNoteData>& notes) override {
        methods.emplace_back("add_new_notes");
        if (inject_failure("add_new_notes")) return injected("add_new_notes");
        auto slot = clip_path.to_string();
        if (!slot.ends_with("/clip")) return rejected("not a clip path");
        slot.resize(slot.size() - std::string_view{"/clip"}.size());
        const auto clip = clips.find(slot);
        if (clip == clips.end()) return rejected("clip slot is empty");
        for (const auto& note : notes)
            clip->second.push_back(static_cast<int>(note.pitch));
        return {true, std::nullopt, std::nullopt};
    }

    [[nodiscard]] bool is_connected() const override { return true; }

    [[nodiscard]] std::size_t total_notes() const {
        std::size_t total = 0;
        for (const auto& [slot, pitches] : clips)
            total += pitches.size();
        return total;
    }

    [[nodiscard]] std::size_t count(std::string_view method) const {
        return static_cast<std::size_t>(std::ranges::count(methods, method));
    }

  private:
    bool inject_failure(const std::string& method) {
        const auto found = failures.find(method);
        if (found == failures.end() || found->second <= 0) return false;
        --found->second;
        return true;
    }

    [[nodiscard]] LomResponse injected(const std::string& method) const {
        return {false, std::nullopt, "injected " + method + " failure", failure_delivery};
    }

    static LomResponse rejected(std::string reason) {
        return {false, std::nullopt, std::move(reason)};
    }
};

const std::string SLOT_0 = "song/tracks/0/clip_slots/0";
const std::string SLOT_1 = "song/tracks/0/clip_slots/1";

json progression_arguments(int slot) {
    return {{"track_index", 0},
            {"slot_index", slot},
            {"root", "C"},
            {"scale", "major"},
            {"numerals", {"I", "IV", "V"}}};
}

json euclidean_arguments(int slot) {
    return {{"track_index", 0}, {"slot_index", slot}, {"pulses", 3}, {"steps", 8}};
}

json arpeggio_arguments(int slot) {
    return {{"track_index", 0},
            {"slot_index", slot},
            {"root", "C"},
            {"scale", "major"},
            {"numerals", {"I", "IV"}},
            {"direction", "up"}};
}

} // namespace

TEST_CASE("creation on an occupied slot leaves the user's clip and the undo history untouched",
          "[mcp][integration][bridge][undo]") {
    const std::vector<std::pair<std::string, json>> operations = {
        {"create_progression_clip", progression_arguments(0)},
        {"apply_euclidean_rhythm", euclidean_arguments(0)},
        {"apply_arpeggio", arpeggio_arguments(0)}};

    for (const auto& [tool, arguments] : operations) {
        CAPTURE(tool);
        Orchestrator orchestrator;
        ClipSlotModelTransport transport;
        transport.clips[SLOT_0] = {48, 55};
        BridgeDispatcher dispatcher(&transport);
        McpServer server;
        register_sunny_tools(server, orchestrator, dispatcher);

        const auto created = call_tool(server, tool, arguments, 70);
        CHECK(created["success"] == false);
        CHECK(created["outcome"] == "not_applied");
        CHECK(transport.clips.at(SLOT_0) == std::vector<int>{48, 55});
        CHECK(transport.count("add_new_notes") == 0);
        CHECK_FALSE(orchestrator.can_undo());

        const auto undone = call_tool(server, "undo_ableton_operation", json::object(), 71);
        CHECK(undone["success"] == false);
        REQUIRE(transport.clips.contains(SLOT_0));
        CHECK(transport.clips.at(SLOT_0) == std::vector<int>{48, 55});
        CHECK(transport.count("delete_clip") == 0);
    }
}

TEST_CASE("a failed undo keeps its entry so a retry reverts the intended operation",
          "[mcp][integration][bridge][undo]") {
    Orchestrator orchestrator;
    ClipSlotModelTransport transport;
    BridgeDispatcher dispatcher(&transport);
    McpServer server;
    register_sunny_tools(server, orchestrator, dispatcher);

    // E(3,8) writes 3 notes into slot 0; the I-IV-V triads write 9 notes into slot 1.
    REQUIRE(call_tool(server, "apply_euclidean_rhythm", euclidean_arguments(0), 80)["success"] ==
            true);
    REQUIRE(call_tool(server, "create_progression_clip", progression_arguments(1), 81)["success"] ==
            true);
    REQUIRE(transport.clips.at(SLOT_0).size() == 3);
    REQUIRE(transport.clips.at(SLOT_1).size() == 9);

    transport.failures["delete_clip"] = 1;
    const auto failed = call_tool(server, "undo_ableton_operation", json::object(), 82);
    CHECK(failed["success"] == false);
    CHECK(failed["can_undo"] == true);
    CHECK(failed["can_redo"] == false);
    CHECK(transport.clips.at(SLOT_1).size() == 9);

    const auto retried = call_tool(server, "undo_ableton_operation", json::object(), 83);
    CHECK(retried["success"] == true);
    CHECK_FALSE(transport.clips.contains(SLOT_1));
    REQUIRE(transport.clips.contains(SLOT_0));
    CHECK(transport.clips.at(SLOT_0).size() == 3);
    CHECK(retried["can_undo"] == true);
    CHECK(retried["can_redo"] == true);

    const auto redone = call_tool(server, "redo_ableton_operation", json::object(), 84);
    CHECK(redone["success"] == true);
    REQUIRE(transport.clips.contains(SLOT_1));
    CHECK(transport.clips.at(SLOT_1).size() == 9);
    CHECK(transport.total_notes() == 12);
    CHECK_FALSE(orchestrator.can_redo());
}

TEST_CASE("a failed note write is compensated by deleting the clip Sunny created",
          "[mcp][integration][bridge][undo]") {
    Orchestrator orchestrator;
    ClipSlotModelTransport transport;
    BridgeDispatcher dispatcher(&transport);
    McpServer server;
    register_sunny_tools(server, orchestrator, dispatcher);

    transport.failures["add_new_notes"] = 1;
    const auto result = call_tool(server, "create_progression_clip", progression_arguments(0), 90);

    CHECK(result["success"] == false);
    CHECK(result["outcome"] == "rolled_back");
    CHECK_FALSE(transport.clips.contains(SLOT_0));
    CHECK(transport.count("delete_clip") == 1);
    CHECK_FALSE(orchestrator.can_undo());
    CHECK_FALSE(orchestrator.can_redo());
}

TEST_CASE("a failed compensation is reported as partially applied without history",
          "[mcp][integration][bridge][undo]") {
    Orchestrator orchestrator;
    ClipSlotModelTransport transport;
    BridgeDispatcher dispatcher(&transport);
    McpServer server;
    register_sunny_tools(server, orchestrator, dispatcher);

    transport.failures["add_new_notes"] = 1;
    transport.failures["delete_clip"] = 1;
    const auto result = call_tool(server, "apply_euclidean_rhythm", euclidean_arguments(0), 91);

    CHECK(result["success"] == false);
    CHECK(result["outcome"] == "partially_applied");
    REQUIRE(result["errors"].is_array());
    CHECK(result["errors"].size() >= 2);
    REQUIRE(transport.clips.contains(SLOT_0));
    CHECK(transport.clips.at(SLOT_0).empty());
    CHECK_FALSE(orchestrator.can_undo());
}

TEST_CASE("a create_clip without a valid response is indeterminate and never compensated",
          "[mcp][integration][bridge][undo]") {
    Orchestrator orchestrator;
    ClipSlotModelTransport transport;
    BridgeDispatcher dispatcher(&transport);
    McpServer server;
    register_sunny_tools(server, orchestrator, dispatcher);

    transport.failures["create_clip"] = 1;
    transport.failure_delivery = LomDeliveryState::SentWithoutValidResponse;
    const auto result = call_tool(server, "create_progression_clip", progression_arguments(0), 92);

    CHECK(result["success"] == false);
    CHECK(result["outcome"] == "indeterminate");
    CHECK(transport.count("add_new_notes") == 0);
    CHECK(transport.count("delete_clip") == 0);
    CHECK_FALSE(orchestrator.can_undo());
}

TEST_CASE("an undo without a valid response is indeterminate and keeps its entry",
          "[mcp][integration][bridge][undo]") {
    Orchestrator orchestrator;
    ClipSlotModelTransport transport;
    BridgeDispatcher dispatcher(&transport);
    McpServer server;
    register_sunny_tools(server, orchestrator, dispatcher);

    REQUIRE(call_tool(server, "apply_euclidean_rhythm", euclidean_arguments(0), 97)["success"] ==
            true);
    transport.failures["delete_clip"] = 1;
    transport.failure_delivery = LomDeliveryState::SentWithoutValidResponse;

    const auto result = call_tool(server, "undo_ableton_operation", json::object(), 98);

    CHECK(result["success"] == false);
    CHECK(result["outcome"] == "indeterminate");
    CHECK(result["can_undo"] == true);
    CHECK(result["can_redo"] == false);
    CHECK(transport.clips.at(SLOT_0).size() == 3);
}

TEST_CASE("a failed redo keeps its entry on the redo stack", "[mcp][integration][bridge][undo]") {
    Orchestrator orchestrator;
    ClipSlotModelTransport transport;
    BridgeDispatcher dispatcher(&transport);
    McpServer server;
    register_sunny_tools(server, orchestrator, dispatcher);

    REQUIRE(call_tool(server, "apply_euclidean_rhythm", euclidean_arguments(0), 93)["success"] ==
            true);
    REQUIRE(call_tool(server, "undo_ableton_operation", json::object(), 94)["success"] == true);

    // The user records a clip into the freed slot before asking Sunny to redo.
    transport.clips[SLOT_0] = {40};
    const auto blocked = call_tool(server, "redo_ableton_operation", json::object(), 95);
    CHECK(blocked["success"] == false);
    CHECK(blocked["can_redo"] == true);
    CHECK(blocked["can_undo"] == false);
    CHECK(transport.clips.at(SLOT_0) == std::vector<int>{40});

    transport.clips.erase(SLOT_0);
    const auto redone = call_tool(server, "redo_ableton_operation", json::object(), 96);
    CHECK(redone["success"] == true);
    CHECK(transport.clips.at(SLOT_0).size() == 3);
    CHECK(redone["can_undo"] == true);
    CHECK(redone["can_redo"] == false);
}

TEST_CASE("offline Ableton tools report the actual reason they cannot connect",
          "[mcp][integration][bridge][offline]") {
    SECTION("no host configured") {
        Orchestrator orchestrator;
        BridgeDispatcher dispatcher(nullptr);
        McpServer server;
        register_sunny_tools(server, orchestrator, dispatcher);

        const auto result = call_tool(server, "undo_ableton_operation", json::object(), 120);
        CHECK(result["success"] == false);
        CHECK(result["error"].get<std::string>().find("SUNNY_ABLETON_HOST is not set") !=
              std::string::npos);
    }

    SECTION("host configured but nothing listening") {
        TcpConfig config;
        config.host = "127.0.0.1";
        config.port = 1; // reserved port: connection refused
        config.connect_timeout = std::chrono::milliseconds{500};
        TcpTransport transport(config);
        Orchestrator orchestrator;
        BridgeDispatcher dispatcher(transport);
        McpServer server;
        register_sunny_tools(server, orchestrator, dispatcher);

        const auto result =
            call_tool(server, "apply_euclidean_rhythm", euclidean_arguments(0), 121);
        CHECK(result["success"] == false);
        const auto error = result["error"].get<std::string>();
        CHECK(error.find("SUNNY_ABLETON_HOST") == std::string::npos);
        REQUIRE(transport.last_connect_failure());
        CHECK(error.find(describe(*transport.last_connect_failure())) != std::string::npos);
    }

    SECTION("a transport without connection diagnostics") {
        TcpConfig config;
        config.host = "127.0.0.1";
        config.port = 1;
        config.connect_timeout = std::chrono::milliseconds{500};
        TcpTransport transport(config);
        Orchestrator orchestrator;
        BridgeDispatcher dispatcher(static_cast<LomTransport*>(&transport));
        McpServer server;
        register_sunny_tools(server, orchestrator, dispatcher);

        const auto result = call_tool(server, "redo_ableton_operation", json::object(), 122);
        CHECK(result["success"] == false);
        CHECK(result["error"].get<std::string>().find("Set SUNNY_ABLETON_HOST") ==
              std::string::npos);
    }
}

namespace {

/// Call apply_arpeggio against a recording transport and return its two commands.
std::pair<CommandBuffer::Entry, CommandBuffer::Entry> record_arpeggio(const json& arguments) {
    Orchestrator orchestrator;
    CommandBuffer transport;
    BridgeDispatcher dispatcher(&transport);
    McpServer server;
    register_sunny_tools(server, orchestrator, dispatcher);

    const auto result = call_tool(server, "apply_arpeggio", arguments, 110);
    INFO(result.dump());
    REQUIRE(result["success"] == true);
    REQUIRE(transport.entries().size() == 2);
    return {transport.entries()[0], transport.entries()[1]};
}

std::vector<int> recorded_pitches(const CommandBuffer::Entry& entry) {
    std::vector<int> pitches;
    for (const auto& note : entry.notes)
        pitches.push_back(static_cast<int>(note.pitch));
    return pitches;
}

} // namespace

TEST_CASE("apply_arpeggio arpeggiates each chord in turn", "[mcp][tools][arpeggio]") {
    const auto [create, notes] = record_arpeggio({{"track_index", 0},
                                                  {"slot_index", 0},
                                                  {"root", "C"},
                                                  {"scale", "major"},
                                                  {"numerals", {"I", "IV", "V"}},
                                                  {"direction", "up"}});

    // C-E-G, then F-A-C, then G-B-D, each an ascending triad from octave 4.
    CHECK(recorded_pitches(notes) == std::vector<int>{60, 64, 67, 65, 69, 72, 67, 71, 74});
    for (std::size_t step = 0; step < notes.notes.size(); ++step) {
        CAPTURE(step);
        CHECK(notes.notes[step].start_time == Catch::Approx(0.25 * static_cast<double>(step)));
    }
    // Nine steps of 0.25 beats loop with period 2.25 beats.
    CHECK(std::get<double>(create.request.args.at(0)) == Catch::Approx(2.25));
}

TEST_CASE("apply_arpeggio clip length is steps times step duration", "[mcp][tools][arpeggio]") {
    const auto [create, notes] = record_arpeggio({{"track_index", 0},
                                                  {"slot_index", 0},
                                                  {"root", "C"},
                                                  {"scale", "major"},
                                                  {"numerals", {"I", "IV"}},
                                                  {"direction", "up"}});

    REQUIRE(notes.notes.size() == 6);
    CHECK(std::get<double>(create.request.args.at(0)) == Catch::Approx(1.5));
}

TEST_CASE("apply_arpeggio honours the requested key", "[mcp][tools][arpeggio]") {
    const auto [create, notes] = record_arpeggio({{"track_index", 0},
                                                  {"slot_index", 0},
                                                  {"root", "A"},
                                                  {"scale", "minor"},
                                                  {"numerals", {"i", "iv"}},
                                                  {"direction", "order"},
                                                  {"step_duration", 0.5}});

    std::vector<int> expected;
    const auto minor = sunny::core::find_scale("minor");
    REQUIRE(minor);
    for (const auto* numeral : {"i", "iv"}) {
        const auto chord = sunny::core::generate_chord_from_numeral(
            numeral, sunny::core::PitchClass{9}, minor->get_intervals(), 4);
        REQUIRE(chord);
        for (const auto pitch : chord->notes)
            expected.push_back(static_cast<int>(pitch));
    }
    // A minor i and iv are A-C-E and D-F-A; none of those is a C major triad.
    CHECK(recorded_pitches(notes) == expected);
    CHECK(recorded_pitches(notes).front() % 12 == 9);
    CHECK(std::get<double>(create.request.args.at(0)) == Catch::Approx(3.0));
}

TEST_CASE("get_scale_notes returns exactly the scale's notes for every built-in scale",
          "[mcp][tools][scale]") {
    Orchestrator orchestrator;
    BridgeDispatcher dispatcher(nullptr);
    McpServer server;
    register_sunny_tools(server, orchestrator, dispatcher);

    const auto major =
        call_tool(server, "get_scale_notes", {{"root", "C"}, {"scale", "major"}}, 60);
    CHECK(major["notes"] == json::array({60, 62, 64, 65, 67, 69, 71}));

    const auto names = sunny::core::list_scale_names();
    REQUIRE(names.size() == 37);
    int id = 61;
    for (const auto name : names) {
        CAPTURE(name);
        const auto definition = sunny::core::find_scale(name);
        REQUIRE(definition);
        const auto result = call_tool(
            server, "get_scale_notes", {{"root", "D"}, {"scale", std::string(name)}}, id++);
        REQUIRE(result["notes"].size() == definition->note_count);
        for (std::size_t degree = 0; degree < definition->note_count; ++degree) {
            CHECK(result["notes"][degree] == 62 + definition->intervals[degree]);
        }
    }
}

TEST_CASE("session-state tool exposes live perception", "[mcp][integration][bridge]") {
    Orchestrator orchestrator;
    SessionTransport transport;
    BridgeDispatcher dispatcher(&transport);
    McpServer server;
    register_sunny_tools(server, orchestrator, dispatcher);

    auto response = server.process_request({{"jsonrpc", "2.0"},
                                            {"method", "tools/call"},
                                            {"params", {{"name", "get_ableton_session_state"}}},
                                            {"id", 44}});
    auto state = json::parse(response["result"]["content"][0]["text"].get<std::string>());

    CHECK(state["success"] == true);
    CHECK(state["connected"] == true);
    CHECK(state["tempo"] == 128.0);
    CHECK(state["signature_numerator"] == 7);
    CHECK(state["signature_denominator"] == 8);
    CHECK(state["is_playing"] == true);
    CHECK(state["current_song_time"] == 12.5);
    CHECK(state["track_count"] == 3);
    CHECK(state["return_track_count"] == 2);
    CHECK(state["target_profile"]["live"]["version"]["string"] == "12.3.5");
    CHECK(state["target_profile"]["adapter"]["contract"] == "version_coupled_private");
}

TEST_CASE("Score IR deploys to Ableton through its public MCP tool",
          "[mcp][integration][score][ableton]") {
    CommandBuffer transport;
    McpServer server;
    register_score_tools(server, &transport);

    auto created = call_tool(server,
                             "score_create",
                             {{"title", "MCP Score"},
                              {"total_bars", 1},
                              {"parts", {{{"name", "Piano"}, {"instrument_type", 0}}}}},
                             50);
    REQUIRE_FALSE(call_tool(server,
                            "score_set_formal_plan",
                            {{"score_id", created["score_id"]},
                             {"sections", {{{"label", "A"}, {"start_bar", 1}, {"end_bar", 1}}}}},
                            501)
                      .contains("error"));
    REQUIRE(call_tool(server,
                      "score_insert_note",
                      {{"score_id", created["score_id"]},
                       {"part_id", created["part_ids"][0]},
                       {"bar", 1},
                       {"pitch", {{"letter", "C"}, {"accidental", 0}, {"octave", 4}}},
                       {"duration", {{"n", 1}, {"d", 4}}},
                       {"velocity", 80},
                       {"release_velocity", 23}},
                      502)["ok"] == true);
    const auto authored =
        call_tool(server, "score_get_json", {{"score_id", created["score_id"]}}, 5021);
    const auto event_id = authored["parts"][0]["measures"][0]["voices"][0]["events"][0]["id"];
    REQUIRE(
        call_tool(
            server,
            "score_modify_note",
            {{"score_id", created["score_id"]}, {"event_id", event_id}, {"release_velocity", 91}},
            5022)["ok"] == true);
    auto compiled = call_tool(
        server, "score_compile_to_ableton", {{"score_id", created["score_id"]}, {"ppq", 480}}, 51);

    CHECK(compiled["success"] == true);
    CHECK(compiled["complete"] == false);
    CHECK(compiled["tracks_created"] == 1);
    CHECK(compiled["clips_created"] == 1);
    CHECK(compiled["notes_requested"] == 1);
    CHECK(compiled["notes_written"] == 1);
    CHECK(compiled["note_batches_requested"] == 1);
    CHECK(compiled["note_batches_executed"] == 0);
    CHECK(compiled["note_ids_returned"] == 0);
    CHECK(compiled["note_batches_verified"] == 0);
    CHECK(compiled["notes_verified"] == 0);
    REQUIRE(compiled["note_deployments"].size() == 1);
    CHECK(compiled["note_deployments"][0]["notes_requested"] == 1);
    REQUIRE(compiled["note_deployments"][0]["requested_notes"].size() == 1);
    CHECK(compiled["note_deployments"][0]["requested_notes"][0] ==
          json{{"pitch", 60},
               {"start_time", 0.0},
               {"duration", 1.0},
               {"velocity", 80.0},
               {"mute", false},
               {"probability", 1.0},
               {"velocity_deviation", 0.0},
               {"release_velocity", 91.0}});
    CHECK(compiled["note_deployments"][0]["action"] == "recorded_only");
    CHECK(compiled["tempo_events_requested"] == 1);
    CHECK(compiled["time_signature_events_requested"] == 1);
    CHECK(compiled["time_signature_events_written"] == 1);
    CHECK(compiled["time_signature_groupings_requested"] == 0);
    CHECK(compiled["time_signature_groupings_written"] == 0);
    CHECK(compiled["report"]["time_signature_events_requested"] == 1);
    CHECK(compiled["report"]["time_signature_events_written"] == 1);
    CHECK(compiled["report"]["time_signature_groupings_requested"] == 0);
    CHECK(compiled["report"]["time_signature_groupings_written"] == 0);
    CHECK(compiled["report"]["has_drops"] == false);
    CHECK(compiled["report"]["has_residuals"] == false);
    CHECK(compiled["key_signature_events_requested"] == 1);
    CHECK(compiled["key_signature_events_written"] == 0);
    CHECK(compiled["section_nodes_total"] == 1);
    CHECK(compiled["section_nodes_projected"] == 1);
    CHECK(compiled["section_nodes_unprojected"] == 0);
    CHECK(compiled["markers_requested"] == 1);
    CHECK(compiled["markers_created"] == 0);
    CHECK(compiled["markers_updated"] == 0);
    CHECK(compiled["markers_verified"] == 0);
    REQUIRE(compiled["marker_deployments"].size() == 1);
    CHECK(compiled["marker_deployments"][0]["requested_name"] == "A");
    CHECK(compiled["marker_deployments"][0]["observed_time"].is_null());
    CHECK(compiled["marker_deployments"][0]["observed_name"].is_null());
    CHECK(compiled["marker_deployments"][0]["action"] == "recorded_only");
    CHECK(compiled["marker_deployments"][0]["verified"] == false);
    CHECK(compiled["property_writes"] == 23);
    CHECK(compiled["property_writes_verified"] == 0);
    REQUIRE(compiled["property_deployments"].size() == 23);
    CHECK(compiled["property_deployments"][0]["property"] == "tempo");
    CHECK(compiled["property_deployments"][0]["observed"].is_null());
    CHECK(compiled["property_deployments"][0]["verified"] == false);
    CHECK(compiled["clip_envelope_clears_requested"] == 1);
    CHECK(compiled["clip_envelope_clears_executed"] == 0);
    CHECK(compiled["clip_envelope_clears_verified"] == 0);
    REQUIRE(compiled["clip_envelope_deployments"].size() == 1);
    CHECK(compiled["clip_envelope_deployments"][0]["action"] == "recorded_only");
    CHECK(compiled["clip_envelope_deployments"][0]["observed_has_envelopes"].is_null());
    CHECK(compiled["target_profile"]["live"]["version"]["major"] == 12);
    CHECK(compiled["target_profile"]["capabilities"]["clip_add_new_notes"] == "available");
    REQUIRE_FALSE(transport.entries().empty());
    CHECK(transport.entries()[0].request.property_or_method == "tempo");
    CHECK(transport.entries()[1].request.property_or_method == "signature_numerator");
}

TEST_CASE("Score MCP authors typed harmony and exports structured MusicXML",
          "[mcp][integration][score][musicxml][harmony]") {
    McpServer server;
    register_score_tools(server);

    const auto created =
        call_tool(server,
                  "score_create",
                  {{"total_bars", 1}, {"parts", {{{"name", "Piano"}, {"instrument_type", 0}}}}},
                  505);
    REQUIRE(created.contains("score_id"));
    const json chord = {{"score_id", created["score_id"]},
                        {"part_id", created["part_ids"][0]},
                        {"bar", 1},
                        {"offset", {{"n", 1}, {"d", 2}}},
                        {"root", {{"letter", "G"}, {"accidental", 0}, {"octave", 4}}},
                        {"quality", "dominant"},
                        {"bass", {{"letter", "B"}, {"accidental", 0}, {"octave", 3}}},
                        {"roman", "V65"},
                        {"numeral", {{"root", 5}, {"key", {{"fifths", 0}, {"mode", 0}}}}},
                        {"inversion", 1},
                        {"degrees", {{{"value", 9}, {"alteration", -1}, {"type", 0}}}}};
    REQUIRE(call_tool(server, "score_insert_chord_symbol", chord, 506)["ok"] == true);

    const auto authored =
        call_tool(server, "score_get_json", {{"score_id", created["score_id"]}}, 507);
    REQUIRE(authored["schema_version"] == 8);
    const auto& event = authored["parts"][0]["measures"][0]["voices"][0]["events"][1];
    CHECK(event["numeral"]["root"] == 5);
    CHECK(event["inversion"] == 1);
    CHECK(event["degrees"][0]["alteration"] == -1);

    const auto compiled =
        call_tool(server, "score_compile_to_musicxml", {{"score_id", created["score_id"]}}, 508);
    CHECK(compiled["xml"].get<std::string>().find("<numeral-root text=\"V65\">5</numeral-root>") !=
          std::string::npos);
    REQUIRE(compiled["report"]["diagnostics"].size() == 1);
    CHECK(compiled["report"]["diagnostics"][0]["message"] ==
          "Stale harmonic-analysis region state was not represented in the MusicXML notation "
          "profile");

    auto contradictory = chord;
    contradictory["root"]["letter"] = "C";
    CHECK(call_tool(server, "score_insert_chord_symbol", contradictory, 509).contains("error"));
    const auto unchanged =
        call_tool(server, "score_get_json", {{"score_id", created["score_id"]}}, 510);
    CHECK(unchanged["version"] == authored["version"]);
    CHECK(unchanged["parts"][0]["measures"][0]["voices"][0]["events"].size() == 2);
}

TEST_CASE("Score Ableton MCP completeness includes MIDI mode-loss evidence",
          "[mcp][integration][score][ableton][key]") {
    CommandBuffer transport;
    McpServer server;
    auto session = std::make_shared<ScoreSession>();
    register_score_tools(server, &transport, session);

    const auto created = call_tool(server,
                                   "score_create",
                                   {{"title", "Modal MCP Score"},
                                    {"total_bars", 1},
                                    {"parts", {{{"name", "Piano"}, {"instrument_type", 0}}}}},
                                   511);
    auto* score = session->find(created["score_id"].get<std::uint64_t>());
    REQUIRE(score != nullptr);
    const auto dorian = sunny::core::find_scale("dorian");
    REQUIRE(dorian.has_value());
    score->key_map[0].key =
        sunny::core::KeySignature{sunny::core::SpelledPitch{1, 0, 4}, *dorian, 0};

    const auto compiled =
        call_tool(server, "score_compile_to_ableton", {{"score_id", created["score_id"]}}, 512);
    CHECK(compiled["success"] == true);
    CHECK(compiled["complete"] == false);
    REQUIRE_FALSE(compiled["warnings"].empty());
    CHECK(compiled["warnings"][0].get<std::string>().find("key signature intent") !=
          std::string::npos);
    REQUIRE(compiled["report"]["diagnostics"].size() == 1);
    CHECK(compiled["report"]["has_drops"] == false);
    CHECK(compiled["report"]["has_residuals"] == true);
    CHECK(compiled["report"]["diagnostics"][0]["message"].get<std::string>().find("dorian") !=
          std::string::npos);
}

TEST_CASE("Score Ableton MCP completeness includes unrepresentable SMF key metadata",
          "[mcp][integration][score][ableton][key]") {
    CommandBuffer transport;
    McpServer server;
    auto session = std::make_shared<ScoreSession>();
    register_score_tools(server, &transport, session);

    const auto created = call_tool(server,
                                   "score_create",
                                   {{"title", "Wide Key MCP Score"},
                                    {"total_bars", 1},
                                    {"parts", {{{"name", "Piano"}, {"instrument_type", 0}}}}},
                                   513);
    auto* score = session->find(created["score_id"].get<std::uint64_t>());
    REQUIRE(score != nullptr);
    const auto major = sunny::core::find_scale("major");
    REQUIRE(major.has_value());
    score->key_map[0].key =
        sunny::core::KeySignature{sunny::core::SpelledPitch{0, 2, 4}, *major, 14};

    const auto compiled =
        call_tool(server, "score_compile_to_ableton", {{"score_id", created["score_id"]}}, 514);
    CHECK(compiled["success"] == true);
    CHECK(compiled["complete"] == false);
    CHECK(compiled["report"]["dropped_key_sig_events"] == 1);
    CHECK(compiled["report"]["has_drops"] == true);
    CHECK(compiled["report"]["has_residuals"] == true);
    REQUIRE(compiled["report"]["diagnostics"].size() == 1);
    CHECK(compiled["report"]["diagnostics"][0]["message"].get<std::string>().find("14 fifths") !=
          std::string::npos);
}

TEST_CASE("Timbre IR deploys to Ableton through its public MCP tool",
          "[mcp][integration][timbre][ableton]") {
    CommandBuffer transport;
    McpServer server;
    register_timbre_tools(server, &transport);

    auto created =
        call_tool(server, "create_timbre_profile", {{"part_id", 1}, {"name", "Piano"}}, 52);
    auto compiled = call_tool(
        server, "compile_timbre", {{"profile_id", created["profile_id"]}, {"track_index", 0}}, 53);

    CHECK(compiled["success"] == true);
    CHECK(compiled["complete"] == false);
    CHECK(compiled["devices_requested"] == 1);
    CHECK(compiled["devices_created"] == 1);
    CHECK(compiled["devices_verified"] == 0);
    REQUIRE(compiled["device_deployments"].size() == 1);
    CHECK(compiled["device_deployments"][0].size() == 23);
    CHECK(compiled["device_deployments"][0]["requested_name"] == "Analog");
    CHECK(compiled["device_deployments"][0]["observed_type"].is_null());
    CHECK(compiled["device_deployments"][0]["observed_latency_in_samples"].is_null());
    CHECK(compiled["device_deployments"][0]["observed_latency_in_ms"].is_null());
    CHECK(compiled["device_deployments"][0]["reported_latency_observed"] == false);
    CHECK(compiled["device_deployments"][0]["render_path_latency_fully_observed"] == false);
    CHECK(compiled["device_deployments"][0]["observed_track_has_audio_output"].is_null());
    CHECK(compiled["device_deployments"][0]["output_verified"] == false);
    CHECK(compiled["device_deployments"][0]["verified"] == false);
    CHECK(compiled["effects_requested"] == 0);
    CHECK(compiled["effects_inserted"] == 0);
    CHECK(compiled["effects_verified"] == 0);
    CHECK(compiled["target_profile"]["capabilities"]["track_insert_device_native"] == "available");
    REQUIRE(compiled["warnings"].size() == 1);
    CHECK(compiled["warnings"][0].get<std::string>().find("source-specific parameter values") !=
          std::string::npos);
    REQUIRE(transport.entries().size() == 1);
    CHECK(transport.entries()[0].request.property_or_method == "insert_device");
}

TEST_CASE("Timbre MCP declares parameter domains and exposes deployment evidence",
          "[mcp][integration][timbre][mapping]") {
    CommandBuffer transport;
    McpServer server;
    register_timbre_tools(server, &transport);

    const auto created =
        call_tool(server, "create_timbre_profile", {{"part_id", 1}, {"name", "Mapped"}}, 530);
    const auto invalid = call_tool(server,
                                   "map_timbre_parameter",
                                   {{"profile_id", created["profile_id"]},
                                    {"ir_path", "source.filter.cutoff"},
                                    {"device_index", 1},
                                    {"parameter_name", "Filter Freq"},
                                    {"source_min", 20.0},
                                    {"source_max", 20000.0},
                                    {"target_min", 0.0},
                                    {"target_max", 1.0}},
                                   531);
    CHECK(invalid["error"].get<std::string>().find("outside") != std::string::npos);
    const auto mapped = call_tool(server,
                                  "map_timbre_parameter",
                                  {{"profile_id", created["profile_id"]},
                                   {"ir_path", "source.filter.cutoff"},
                                   {"device_index", 0},
                                   {"parameter_name", "Filter Freq"},
                                   {"source_min", 20.0},
                                   {"source_max", 20000.0},
                                   {"target_min", 0.0},
                                   {"target_max", 1.0},
                                   {"curve_type", 0},
                                   {"value_property", "value"}},
                                  532);
    REQUIRE(mapped["success"] == true);
    CHECK(mapped["source_value"] == 1000.0);

    const auto compiled = call_tool(
        server, "compile_timbre", {{"profile_id", created["profile_id"]}, {"track_index", 0}}, 533);
    REQUIRE(compiled["success"] == true);
    CHECK(compiled["parameters_mapped"] == 1);
    CHECK(compiled["parameters_verified"] == 0);
    REQUIRE(compiled["parameter_deployments"].size() == 1);
    CHECK(compiled["parameter_deployments"][0].size() == 21);
    CHECK(compiled["parameter_deployments"][0]["ir_path"] == "source.filter.cutoff");
    CHECK(compiled["parameter_deployments"][0]["range_min"] == 0.0);
    CHECK(compiled["parameter_deployments"][0]["range_max"] == 1.0);
    CHECK(compiled["parameter_deployments"][0]["matched_name"].is_null());
    CHECK(compiled["parameter_deployments"][0]["original_name"].is_null());
    CHECK(compiled["parameter_deployments"][0]["observed_value"].is_null());
    CHECK(compiled["parameter_deployments"][0]["observed_minimum"].is_null());
    CHECK(compiled["parameter_deployments"][0]["observed_maximum"].is_null());
    CHECK(compiled["parameter_deployments"][0]["is_quantized"].is_null());
    CHECK(compiled["parameter_deployments"][0]["default_value"].is_null());
    CHECK(compiled["parameter_deployments"][0]["value_items"].is_null());
    CHECK(compiled["parameter_deployments"][0]["is_enabled"].is_null());
    CHECK(compiled["parameter_deployments"][0]["action"] == "recorded_only");
    CHECK(compiled["parameter_deployments"][0]["parameter_state"].is_null());
    CHECK(compiled["parameter_deployments"][0]["automation_state"].is_null());
    CHECK(compiled["parameter_deployments"][0]["verified"] == false);
}

TEST_CASE("Mix IR deploys to Ableton through its public MCP tool",
          "[mcp][integration][mix][ableton]") {
    CommandBuffer transport;
    McpServer server;
    register_mix_tools(server, &transport);

    auto created = call_tool(server, "create_mix_graph", {{"part_ids", {1}}}, 54);
    auto compiled = call_tool(
        server, "compile_mix", {{"graph_id", created["graph_id"]}, {"base_track", 0}}, 55);

    CHECK(compiled["success"] == true);
    CHECK(compiled["complete"] == false);
    CHECK(compiled["group_tracks_requested"] == 0);
    CHECK(compiled["group_tracks_created"] == 0);
    CHECK(compiled["return_tracks_requested"] == 0);
    CHECK(compiled["return_tracks_created"] == 0);
    CHECK(compiled["return_track_deployments"].empty());
    REQUIRE(compiled["master_track_deployment"].is_object());
    CHECK(compiled["master_track_deployment"].size() == 3);
    CHECK(compiled["master_track_deployment"]["requested_track_activator"] == 1.0);
    CHECK(compiled["master_track_deployment"]["requested_panning_mode"] == 0);
    CHECK(compiled["master_track_deployment"]["requested_pan"] == 0.0);
    CHECK(compiled["effects_requested"] == 0);
    CHECK(compiled["effects_inserted"] == 0);
    CHECK(compiled["effects_verified"] == 0);
    CHECK(compiled["device_deployments"].empty());
    CHECK(compiled["sends_requested"] == 0);
    CHECK(compiled["sends_configured"] == 0);
    CHECK(compiled["send_levels_requested"] == 0);
    CHECK(compiled["send_levels_configured"] == 0);
    CHECK(compiled["send_modes_requested"] == 0);
    CHECK(compiled["send_modes_configured"] == 0);
    CHECK(compiled["output_routes_requested"] == 1);
    CHECK(compiled["output_routes_written"] == 0);
    CHECK(compiled["output_routes_verified"] == 0);
    CHECK(compiled["output_routing_bindings"] ==
          json{{"part_tracks", json::array()}, {"aux_returns", json::array()}});
    REQUIRE(compiled["output_route_deployments"].size() == 1);
    CHECK(compiled["output_route_deployments"][0]["binding"].is_null());
    CHECK(compiled["output_route_deployments"][0]["action"] == "not_applied");
    REQUIRE(compiled["output_route_residuals"].size() == 1);
    CHECK(compiled["channels_requested"] == 1);
    CHECK(compiled["channels_configured"] == 1);
    CHECK(compiled["property_writes"] == 9);
    CHECK(compiled["property_writes_verified"] == 0);
    REQUIRE(compiled["property_deployments"].size() == 9);
    CHECK(compiled["property_deployments"][0]["observed"].is_null());
    CHECK(compiled["target_profile"]["capabilities"]["max_for_live"] == "unknown");
    REQUIRE(transport.entries().size() == 9);
    CHECK(transport.entries()[0].request.path.to_string() == "song/tracks/0/mixer_device/volume");
    CHECK(transport.entries()[1].request.path.to_string() == "song/tracks/0/mixer_device/panning");
    CHECK(transport.entries()[2].request.property_or_method == "mute");
    CHECK(std::get<bool>(transport.entries()[2].request.args[0]) == false);
    CHECK(transport.entries()[3].request.property_or_method == "solo");
    CHECK(std::get<bool>(transport.entries()[3].request.args[0]) == false);
    CHECK(transport.entries()[4].request.path.to_string() ==
          "song/tracks/0/mixer_device/track_activator");
    CHECK(transport.entries()[5].request.path.to_string() ==
          "song/master_track/mixer_device/track_activator");
    CHECK(transport.entries()[6].request.path.to_string() == "song/master_track/mixer_device");
    CHECK(transport.entries()[6].request.property_or_method == "panning_mode");
    CHECK(transport.entries()[7].request.path.to_string() ==
          "song/master_track/mixer_device/panning");
    CHECK(transport.entries()[8].request.path.to_string() ==
          "song/master_track/mixer_device/volume");
}

TEST_CASE("Mix MCP admits exact route dictionaries with retained semantic provenance",
          "[mcp][integration][mix][routing]") {
    CommandBuffer transport;
    McpServer server;
    register_mix_tools(server, &transport);
    const auto created = call_tool(server, "create_mix_graph", {{"part_ids", {1}}}, 5500);
    const json binding = {{"part_tracks",
                           {{{"part_id", 1},
                             {"type", {{"display_name", "Master"}, {"identifier", "master"}}},
                             {"channel", {{"display_name", "1/2"}, {"identifier", "stereo_1_2"}}},
                             {"mapping_provenance", "named Live/operator fixture"}}}},
                          {"aux_returns", json::array()}};
    const auto compiled = call_tool(server,
                                    "compile_mix",
                                    {{"graph_id", created["graph_id"]},
                                     {"base_track", 0},
                                     {"output_routing_bindings", binding}},
                                    5501);
    REQUIRE(compiled["success"] == true);
    CHECK(compiled["output_routes_requested"] == 1);
    CHECK(compiled["output_routes_written"] == 1);
    CHECK(compiled["output_routes_verified"] == 0);
    CHECK(compiled["output_routing_bindings"] == binding);
    REQUIRE(compiled["output_route_deployments"].size() == 1);
    const auto& deployment = compiled["output_route_deployments"][0];
    CHECK(deployment["part_id"] == 1);
    CHECK(deployment["binding"]["mapping_provenance"] == "named Live/operator fixture");
    CHECK(deployment["action"] == "recorded_only");
    CHECK(deployment["verified"] == false);
    REQUIRE(transport.entries().size() == 11);
    CHECK(transport.entries()[0].request.property_or_method == "sunny_set_output_routing_type");
    CHECK(transport.entries()[1].request.property_or_method == "sunny_set_output_routing_channel");

    const json duplicate = {
        {"part_tracks", json::array({binding["part_tracks"][0], binding["part_tracks"][0]})},
        {"aux_returns", json::array()}};
    const auto rejected = call_tool(server,
                                    "compile_mix",
                                    {{"graph_id", created["graph_id"]},
                                     {"base_track", 0},
                                     {"output_routing_bindings", duplicate}},
                                    5502);
    CHECK_FALSE(rejected.contains("success"));
    CHECK(rejected["error"].get<std::string>().find("unique uint64 part_id") != std::string::npos);
    CHECK(transport.entries().size() == 11);
}

TEST_CASE("Mix MCP mapping is transactional and exposes deployment coverage",
          "[mcp][integration][mix][device-parameter]") {
    CommandBuffer transport;
    McpServer server;
    register_mix_tools(server, &transport);

    const auto graph = call_tool(server, "create_mix_graph", {{"part_ids", {1}}}, 550);
    const auto effect = call_tool(server,
                                  "add_channel_effect",
                                  {{"graph_id", graph["graph_id"]},
                                   {"channel_id", 1},
                                   {"effect_type", "compressor"},
                                   {"threshold", -20.0}},
                                  551);
    const json mapping = {{"graph_id", graph["graph_id"]},
                          {"effect_id", effect["effect_id"]},
                          {"source_path", "missing"},
                          {"parameter_name", "Threshold"},
                          {"source_min", -60.0},
                          {"source_max", 0.0},
                          {"target_min", 0.0},
                          {"target_max", 1.0}};
    const auto invalid = call_tool(server, "map_mix_effect_parameter", mapping, 552);
    CHECK(invalid.contains("error"));
    auto stored = call_tool(server, "get_mix_json", {{"graph_id", graph["graph_id"]}}, 553);
    CHECK(stored["mix_ir"]["channels"][0]["insert_chain"][0]["parameter_map"].empty());

    auto valid_mapping = mapping;
    valid_mapping["source_path"] = "threshold";
    const auto mapped = call_tool(server, "map_mix_effect_parameter", valid_mapping, 554);
    REQUIRE(mapped["success"] == true);
    CHECK(mapped["source_value"] == -20.0);
    CHECK(mapped["target_value"].get<double>() == Catch::Approx(2.0 / 3.0));

    stored = call_tool(server, "get_mix_json", {{"graph_id", graph["graph_id"]}}, 555);
    CHECK(stored["mix_ir"]["channels"][0]["insert_chain"][0]["parameter_map"].size() == 1);
    const auto compiled =
        call_tool(server, "compile_mix", {{"graph_id", graph["graph_id"]}, {"base_track", 0}}, 556);
    REQUIRE(compiled["success"] == true);
    CHECK(compiled["parameter_sources_total"] == 10);
    CHECK(compiled["parameter_sources_explicitly_mapped"] == 1);
    CHECK(compiled["parameter_sources_unmapped"] == 9);
    CHECK(compiled["parameters_mapped"] == 1);
    CHECK(compiled["parameters_verified"] == 0);
    REQUIRE(compiled["parameter_deployments"].size() == 1);
    CHECK(compiled["parameter_deployments"][0].size() == 22);
    CHECK(compiled["parameter_deployments"][0]["device_path"] == "song/tracks/0/devices/0");
    CHECK(compiled["parameter_deployments"][0]["range_min"] == 0.0);
    CHECK(compiled["parameter_deployments"][0]["range_max"] == 1.0);
    CHECK(compiled["parameter_deployments"][0]["matched_name"].is_null());
    CHECK(compiled["parameter_deployments"][0]["original_name"].is_null());
    CHECK(compiled["parameter_deployments"][0]["observed_value"].is_null());
    CHECK(compiled["parameter_deployments"][0]["observed_minimum"].is_null());
    CHECK(compiled["parameter_deployments"][0]["observed_maximum"].is_null());
    CHECK(compiled["parameter_deployments"][0]["is_quantized"].is_null());
    CHECK(compiled["parameter_deployments"][0]["default_value"].is_null());
    CHECK(compiled["parameter_deployments"][0]["value_items"].is_null());
    CHECK(compiled["parameter_deployments"][0]["is_enabled"].is_null());
    CHECK(compiled["parameter_deployments"][0]["action"] == "recorded_only");
    CHECK(compiled["parameter_deployments"][0]["parameter_state"].is_null());
    CHECK(compiled["parameter_deployments"][0]["automation_state"].is_null());
    REQUIRE(compiled["parameter_coverage"].size() == 1);
    CHECK(compiled["parameter_coverage"][0].size() == 6);
    CHECK(compiled["parameter_coverage"][0]["missing_paths"].size() == 9);
}

TEST_CASE("Mix MCP authors and resolves relative faders transactionally",
          "[mcp][integration][mix][relative-level]") {
    CommandBuffer transport;
    McpServer server;
    register_mix_tools(server, &transport);

    const auto graph = call_tool(server, "create_mix_graph", {{"part_ids", {1, 2}}}, 557);
    REQUIRE(call_tool(server,
                      "set_channel_level",
                      {{"graph_id", graph["graph_id"]}, {"channel_id", 2}, {"level_db", -6.0}},
                      558)["success"] == true);
    const auto relative = call_tool(server,
                                    "set_channel_relative_level",
                                    {{"graph_id", graph["graph_id"]},
                                     {"channel_id", 1},
                                     {"reference_type", "channel"},
                                     {"reference_id", 2},
                                     {"relationship", "3 dB below"},
                                     {"offset_db", -3.0}},
                                    559);
    REQUIRE(relative["success"] == true);
    CHECK(relative["fader_level_resolution"]["complete"] == true);
    CHECK(relative["fader_level_resolution"]["relative_levels_resolved"] == 1);
    CHECK(relative["fader_level_resolution"]["levels"][0]["resolved_level_db"] == -9.0);

    const auto resolved =
        call_tool(server, "resolve_mix_fader_levels", {{"graph_id", graph["graph_id"]}}, 560);
    REQUIRE(resolved["success"] == true);
    CHECK(resolved["fader_level_resolution"]["levels"][0]["status"] == "resolved");
    CHECK(resolved["fader_level_resolution"]["levels"][0]["reference"]["id"] == 2);

    const auto cycle = call_tool(server,
                                 "set_channel_relative_level",
                                 {{"graph_id", graph["graph_id"]},
                                  {"channel_id", 2},
                                  {"reference_type", "channel"},
                                  {"reference_id", 1},
                                  {"offset_db", 0.0}},
                                 561);
    CHECK(cycle.contains("error"));
    const auto after =
        call_tool(server, "resolve_mix_fader_levels", {{"graph_id", graph["graph_id"]}}, 562);
    CHECK(after["fader_level_resolution"]["relative_levels_total"] == 1);

    const auto compiled =
        call_tool(server, "compile_mix", {{"graph_id", graph["graph_id"]}, {"base_track", 0}}, 563);
    REQUIRE(compiled["success"] == true);
    CHECK(compiled["fader_level_resolution"]["relative_levels_resolved"] == 1);
    CHECK(compiled["fader_level_resolution"]["levels"][0]["resolved_level_db"] == -9.0);

    const auto measured = call_tool(server,
                                    "set_channel_relative_level",
                                    {{"graph_id", graph["graph_id"]},
                                     {"channel_id", 1},
                                     {"reference_type", "master_target"},
                                     {"lufs", -16.0},
                                     {"offset_db", 0.0}},
                                    564);
    REQUIRE(measured["success"] == true);
    CHECK(measured["fader_level_resolution"]["complete"] == false);
    CHECK(measured["fader_level_resolution"]["levels"][0]["status"] ==
          "requires_loudness_measurement");
    CHECK(measured["fader_level_resolution"]["levels"][0]["resolved_level_db"].is_null());
}

TEST_CASE("Project MCP validates and deploys shared IR stores by PartId",
          "[mcp][integration][project][ableton]") {
    CommandBuffer transport;
    McpServer server;
    McpSession session;
    register_score_tools(server, &transport, session.score);
    register_timbre_tools(server, &transport, session.timbre);
    register_mix_tools(server, &transport, session.mix);
    register_project_tools(server, session, &transport);

    const auto score = call_tool(
        server,
        "score_create",
        {{"title", "Project"},
         {"total_bars", 1},
         {"parts",
          {{{"name", "One"}, {"instrument_type", 0}}, {{"name", "Two"}, {"instrument_type", 0}}}}},
        560);
    REQUIRE(score["part_ids"].size() == 2);
    REQUIRE_FALSE(call_tool(server,
                            "score_set_formal_plan",
                            {{"score_id", score["score_id"]},
                             {"sections", {{{"label", "A"}, {"start_bar", 1}, {"end_bar", 1}}}}},
                            5601)
                      .contains("error"));
    const auto first_part = score["part_ids"][0];
    const auto second_part = score["part_ids"][1];
    REQUIRE(call_tool(server,
                      "score_insert_note",
                      {{"score_id", score["score_id"]},
                       {"part_id", second_part},
                       {"bar", 1},
                       {"pitch", {{"letter", "C"}, {"accidental", 0}, {"octave", 4}}},
                       {"duration", {{"n", 1}, {"d", 8}}},
                       {"velocity", 72}},
                      5602)["ok"] == true);
    const auto first =
        call_tool(server, "create_timbre_profile", {{"part_id", first_part}, {"name", "One"}}, 561);
    const auto second = call_tool(
        server, "create_timbre_profile", {{"part_id", second_part}, {"name", "Two"}}, 562);
    REQUIRE(call_tool(server,
                      "map_timbre_parameter",
                      {{"profile_id", first["profile_id"]},
                       {"ir_path", "source.filter.cutoff"},
                       {"device_index", 0},
                       {"parameter_name", "Filter Freq"},
                       {"source_min", 20.0},
                       {"source_max", 20000.0},
                       {"target_min", 0.0},
                       {"target_max", 1.0},
                       {"value_property", "value"}},
                      5621)["success"] == true);
    const auto mix =
        call_tool(server, "create_mix_graph", {{"part_ids", {second_part, first_part}}}, 563);
    const auto aux = call_tool(
        server, "create_aux_bus", {{"graph_id", mix["graph_id"]}, {"name", "Hall"}}, 5631);
    REQUIRE(aux["success"] == true);
    REQUIRE(call_tool(server,
                      "set_channel_level",
                      {{"graph_id", mix["graph_id"]}, {"channel_id", 1}, {"level_db", -12.0}},
                      564)["success"] == true);

    const json project_ids = {{"score_id", score["score_id"]},
                              {"timbre_profile_ids", {second["profile_id"], first["profile_id"]}},
                              {"mix_graph_id", mix["graph_id"]}};
    const auto validation = call_tool(server, "project_validate", project_ids, 565);
    CHECK(validation["valid"] == true);

    const auto compiled = call_tool(server, "project_compile_to_ableton", project_ids, 566);
    REQUIRE(compiled["success"] == true);
    CHECK(compiled["score"]["tracks_created"] == 2);
    CHECK(compiled["score"]["notes_requested"] == 1);
    CHECK(compiled["score"]["notes_written"] == 1);
    CHECK(compiled["score"]["note_batches_requested"] == 1);
    CHECK(compiled["score"]["note_batches_executed"] == 0);
    CHECK(compiled["score"]["note_ids_returned"] == 0);
    CHECK(compiled["score"]["note_batches_verified"] == 0);
    CHECK(compiled["score"]["notes_verified"] == 0);
    REQUIRE(compiled["score"]["note_deployments"].size() == 1);
    CHECK(compiled["score"]["note_deployments"][0]["track_index"] == 1);
    REQUIRE(compiled["score"]["note_deployments"][0]["requested_notes"].size() == 1);
    CHECK(compiled["score"]["note_deployments"][0]["requested_notes"][0] ==
          json{{"pitch", 60},
               {"start_time", 0.0},
               {"duration", 0.5},
               {"velocity", 72.0},
               {"mute", false},
               {"probability", 1.0},
               {"velocity_deviation", 0.0},
               {"release_velocity", 64.0}});
    CHECK(compiled["score"]["tempo_events_requested"] == 1);
    CHECK(compiled["score"]["time_signature_events_requested"] == 1);
    CHECK(compiled["score"]["time_signature_events_written"] == 1);
    CHECK(compiled["score"]["time_signature_groupings_requested"] == 0);
    CHECK(compiled["score"]["time_signature_groupings_written"] == 0);
    CHECK(compiled["score"]["key_signature_events_requested"] == 1);
    CHECK(compiled["score"]["key_signature_events_written"] == 0);
    CHECK(compiled["score"]["tuning_definitions_requested"] == 1);
    CHECK(compiled["score"]["tuning_definitions_written"] == 0);
    CHECK(compiled["score"]["requested_tuning"]["cents_from_reference"].size() == 128);
    CHECK(compiled["score"]["report"]["tuning_definitions_requested"] == 1);
    CHECK(compiled["score"]["report"]["tuning_definitions_written"] == 1);
    CHECK(compiled["score"]["report"]["time_signature_events_requested"] == 1);
    CHECK(compiled["score"]["report"]["time_signature_events_written"] == 1);
    CHECK(compiled["score"]["report"]["time_signature_groupings_requested"] == 0);
    CHECK(compiled["score"]["report"]["time_signature_groupings_written"] == 0);
    CHECK(compiled["score"]["report"]["has_drops"] == false);
    CHECK(compiled["score"]["report"]["has_residuals"] == false);
    CHECK(compiled["score"]["section_nodes_total"] == 1);
    CHECK(compiled["score"]["section_nodes_projected"] == 1);
    CHECK(compiled["score"]["section_nodes_unprojected"] == 0);
    CHECK(compiled["score"]["markers_requested"] == 1);
    CHECK(compiled["score"]["markers_created"] == 0);
    CHECK(compiled["score"]["markers_updated"] == 0);
    CHECK(compiled["score"]["markers_verified"] == 0);
    REQUIRE(compiled["score"]["marker_deployments"].size() == 1);
    CHECK(compiled["score"]["marker_deployments"][0]["action"] == "recorded_only");
    CHECK(compiled["score"]["property_writes"] == 40);
    CHECK(compiled["score"]["property_writes_verified"] == 0);
    REQUIRE(compiled["score"]["property_deployments"].size() == 40);
    CHECK(compiled["score"]["clip_envelope_clears_requested"] == 2);
    CHECK(compiled["score"]["clip_envelope_clears_executed"] == 0);
    CHECK(compiled["score"]["clip_envelope_clears_verified"] == 0);
    REQUIRE(compiled["score"]["clip_envelope_deployments"].size() == 2);
    CHECK(compiled["timbre"].size() == 2);
    CHECK(compiled["timbre"][0]["devices_requested"] == 1);
    CHECK(compiled["timbre"][0]["devices_created"] == 1);
    CHECK(compiled["timbre"][0]["devices_verified"] == 0);
    REQUIRE(compiled["timbre"][0]["device_deployments"].size() == 1);
    CHECK(compiled["timbre"][0]["device_deployments"][0]["observed_active"].is_null());
    CHECK(compiled["timbre"][0]["device_deployments"][0]["observed_can_have_chains"].is_null());
    CHECK(compiled["timbre"][0]["device_deployments"][0]["flat_device_verified"] == false);
    CHECK(
        compiled["timbre"][0]["device_deployments"][0]["observed_track_has_midi_output"].is_null());
    CHECK(compiled["timbre"][0]["effects_requested"] == 0);
    CHECK(compiled["timbre"][0]["effects_inserted"] == 0);
    CHECK(compiled["timbre"][0]["effects_verified"] == 0);
    CHECK(compiled["timbre"][0]["parameters_mapped"] == 1);
    CHECK(compiled["timbre"][0]["parameters_verified"] == 0);
    REQUIRE(compiled["timbre"][0]["parameter_deployments"].size() == 1);
    CHECK(compiled["timbre"][0]["parameter_deployments"][0]["action"] == "recorded_only");
    CHECK(compiled["timbre"][0]["parameter_deployments"][0]["range_min"] == 0.0);
    CHECK(compiled["timbre"][0]["parameter_deployments"][0]["range_max"] == 1.0);
    CHECK(compiled["timbre"][0]["parameter_deployments"][0]["matched_name"].is_null());
    CHECK(compiled["timbre"][0]["parameter_deployments"][0]["is_enabled"].is_null());
    CHECK(compiled["timbre"][0]["automation_lanes_requested"] == 0);
    CHECK(compiled["timbre"][0]["automation_lanes_written"] == 0);
    CHECK_FALSE(compiled["timbre"][0].contains("automation_lanes"));
    CHECK(compiled["mix"]["group_tracks_requested"] == 0);
    CHECK(compiled["mix"]["return_tracks_requested"] == 1);
    CHECK(compiled["mix"]["return_tracks_created"] == 1);
    REQUIRE(compiled["mix"]["return_track_deployments"].size() == 1);
    CHECK(compiled["mix"]["return_track_deployments"][0].size() == 9);
    CHECK(compiled["mix"]["return_track_deployments"][0]["aux_bus_id"] == aux["aux_bus_id"]);
    CHECK(compiled["mix"]["return_track_deployments"][0]["track_index"] == 0);
    CHECK(compiled["mix"]["return_track_deployments"][0]["requested_name"] == "Hall");
    CHECK(compiled["mix"]["return_track_deployments"][0]["requested_mute"] == false);
    CHECK(compiled["mix"]["return_track_deployments"][0]["requested_solo"] == false);
    CHECK(compiled["mix"]["return_track_deployments"][0]["requested_crossfade_assign"] == 1);
    CHECK(compiled["mix"]["return_track_deployments"][0]["requested_panning_mode"] == 0);
    CHECK(compiled["mix"]["return_track_deployments"][0]["requested_pan"] == 0.0);
    CHECK(compiled["mix"]["return_track_deployments"][0]["requested_track_activator"] == 1.0);
    CHECK(compiled["mix"]["master_track_deployment"]["requested_track_activator"] == 1.0);
    CHECK(compiled["mix"]["master_track_deployment"]["requested_panning_mode"] == 0);
    CHECK(compiled["mix"]["master_track_deployment"]["requested_pan"] == 0.0);
    CHECK(compiled["mix"]["effects_requested"] == 0);
    CHECK(compiled["mix"]["effects_verified"] == 0);
    CHECK(compiled["mix"]["device_deployments"].empty());
    CHECK(compiled["mix"]["sends_requested"] == 0);
    CHECK(compiled["mix"]["sends_configured"] == 0);
    CHECK(compiled["mix"]["send_levels_requested"] == 0);
    CHECK(compiled["mix"]["send_levels_configured"] == 0);
    CHECK(compiled["mix"]["send_modes_requested"] == 0);
    CHECK(compiled["mix"]["send_modes_configured"] == 0);
    CHECK(compiled["mix"]["output_routes_requested"] == 3);
    CHECK(compiled["mix"]["output_routes_written"] == 0);
    CHECK(compiled["mix"]["output_routes_verified"] == 0);
    REQUIRE(compiled["mix"]["output_route_residuals"].size() == 3);
    CHECK(compiled["mix"]["channels_requested"] == 2);
    CHECK(compiled["mix"]["channels_configured"] == 2);
    CHECK(compiled["mix"]["automation_lanes_requested"] == 0);
    CHECK(compiled["mix"]["automation_lanes_written"] == 0);
    CHECK_FALSE(compiled["mix"].contains("automation_lanes"));
    CHECK(compiled["mix"]["property_writes"] == 22);
    CHECK(compiled["mix"]["property_writes_verified"] == 0);
    CHECK(compiled["mix"]["parameter_sources_total"] == 0);
    CHECK(compiled["mix"]["parameters_mapped"] == 0);
    CHECK(compiled["mix"]["parameter_coverage"].empty());
    CommandBuffer standalone_transport;
    McpServer standalone_server;
    register_timbre_tools(standalone_server, &standalone_transport, session.timbre);
    register_mix_tools(standalone_server, &standalone_transport, session.mix);
    const auto standalone_timbre =
        call_tool(standalone_server,
                  "compile_timbre",
                  {{"profile_id", first["profile_id"]}, {"track_index", 0}},
                  5661);
    REQUIRE(standalone_timbre["success"] == true);
    CHECK(standalone_timbre["device_deployments"] == compiled["timbre"][0]["device_deployments"]);
    CHECK(standalone_timbre["parameter_deployments"] ==
          compiled["timbre"][0]["parameter_deployments"]);
    const auto standalone_mix = call_tool(
        standalone_server, "compile_mix", {{"graph_id", mix["graph_id"]}, {"base_track", 0}}, 5662);
    REQUIRE(standalone_mix["success"] == true);
    CHECK(standalone_mix["return_track_deployments"] ==
          compiled["mix"]["return_track_deployments"]);
    CHECK(standalone_mix["master_track_deployment"] == compiled["mix"]["master_track_deployment"]);
    CHECK(standalone_mix["device_deployments"] == compiled["mix"]["device_deployments"]);
    CHECK(standalone_mix["parameter_deployments"] == compiled["mix"]["parameter_deployments"]);
    CHECK(standalone_mix["parameter_coverage"] == compiled["mix"]["parameter_coverage"]);
    CHECK(compiled["mix"]["fader_level_resolution"]["complete"] == true);
    CHECK(compiled["postconditions"]["observed"] == false);
    CHECK(compiled["postconditions"]["song_states_requested"] == 1);
    CHECK(compiled["postconditions"]["song_states_verified"] == 0);
    CHECK(compiled["postconditions"]["song_state"]["requested_scene_name"] == "Project");
    CHECK(compiled["postconditions"]["song_state"]["requested_scene_triggered"] == false);
    CHECK(compiled["postconditions"]["song_state"]["requested_is_counting_in"] == false);
    CHECK(compiled["postconditions"]["song_state"]["requested_arrangement_overdub"] == false);
    CHECK(compiled["postconditions"]["song_state"]["requested_overdub"] == false);
    CHECK(compiled["postconditions"]["song_state"]["requested_record_mode"] == false);
    CHECK(compiled["postconditions"]["song_state"]["requested_session_record"] == false);
    CHECK(compiled["postconditions"]["song_state"]["requested_session_automation_record"] == false);
    CHECK(compiled["postconditions"]["song_state"]["requested_is_ableton_link_enabled"] == false);
    CHECK(compiled["postconditions"]["song_state"]
                  ["requested_is_ableton_link_start_stop_sync_enabled"] == false);
    CHECK(compiled["postconditions"]["song_state"]["requested_tempo_follower_enabled"] == false);
    CHECK(compiled["postconditions"]["song_state"]["requested_nudge_down"] == false);
    CHECK(compiled["postconditions"]["song_state"]["requested_nudge_up"] == false);
    CHECK(compiled["postconditions"]["song_state"]["requested_back_to_arranger"] == false);
    CHECK(compiled["postconditions"]["song_state"]["requested_re_enable_automation_enabled"] ==
          false);
    CHECK(compiled["postconditions"]["song_state"]["requested_arrangement_loop"] == false);
    CHECK(compiled["postconditions"]["song_state"]["requested_metronome"] == false);
    CHECK(compiled["postconditions"]["song_state"]["observed_scene_triggered_states"].empty());
    CHECK(compiled["postconditions"]["song_state"]["scene_trigger_states_observed"] == false);
    CHECK(compiled["postconditions"]["song_state"]["all_scene_launches_quiescent_verified"] ==
          false);
    CHECK(compiled["postconditions"]["song_state"]["observed_is_playing"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_is_counting_in"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_arrangement_overdub"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_overdub"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_record_mode"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_session_record"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_session_automation_record"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_is_ableton_link_enabled"].is_null());
    CHECK(
        compiled["postconditions"]["song_state"]["observed_is_ableton_link_start_stop_sync_enabled"]
            .is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_tempo_follower_enabled"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_nudge_down"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_nudge_up"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_back_to_arranger"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_re_enable_automation_enabled"]
              .is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_arrangement_loop"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_metronome"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["transport_stopped_verified"] == false);
    CHECK(compiled["postconditions"]["song_state"]["recording_modes_quiescent_verified"] == false);
    CHECK(compiled["postconditions"]["song_state"]["public_tempo_controls_quiescent_verified"] ==
          false);
    CHECK(compiled["postconditions"]["song_state"]["external_midi_sync_state_observed"] == false);
    CHECK(compiled["postconditions"]["song_state"]["tempo_automation_state_observed"] == false);
    CHECK(compiled["postconditions"]["song_state"]["tempo_stability_verified"] == false);
    CHECK(compiled["postconditions"]["song_state"]["arrangement_playback_aligned_verified"] ==
          false);
    CHECK(compiled["postconditions"]["song_state"]["automation_overrides_quiescent_verified"] ==
          false);
    CHECK(compiled["postconditions"]["song_state"]["arrangement_loop_disabled_verified"] == false);
    CHECK(compiled["postconditions"]["song_state"]["metronome_disabled_verified"] == false);
    CHECK(compiled["postconditions"]["song_state"]["observed_tempo"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_scale_name"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_scale_intervals"].empty());
    CHECK(compiled["postconditions"]["song_state"]["observed_tuning_lowest_note"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_tuning_highest_note"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_tuning_reference_pitch"].is_null());
    CHECK(compiled["postconditions"]["song_state"]["observed_tuning_note_tunings"].is_null());
    CHECK_FALSE(compiled["postconditions"]["song_state"]["tuning_dictionary_payloads_observed"]);
    CHECK_FALSE(compiled["postconditions"]["song_state"]["audible_pitch_verified"]);
    CHECK(compiled["postconditions"]["track_gates_requested"] == 2);
    CHECK(compiled["postconditions"]["track_gates_verified"] == 0);
    REQUIRE(compiled["postconditions"]["track_gates"].size() == 2);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_crossfade_assign"] == 1);
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_crossfade_assign"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["crossfade_neutral_verified"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_panning_mode"] == 0);
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_panning_mode"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["stereo_panning_verified"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_track_activator_is_quantized"]
              .is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["track_activator_range_verified"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["track_activator_quantization_verified"] ==
          false);
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_mute"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_has_audio_input"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_has_midi_input"] == true);
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_has_audio_input"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_has_midi_input"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["input_classification_verified"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_input_meter_level"] == 0.0);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_output_meter_level"] == 0.0);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_input_meter_left"] == 0.0);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_input_meter_right"] == 0.0);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_output_meter_left"] == 0.0);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_output_meter_right"] == 0.0);
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_input_meter_level"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_output_meter_level"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_input_meter_left"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_input_meter_right"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_output_meter_left"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_output_meter_right"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["meter_levels_observed"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["meter_hold_quiescence_verified"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["momentary_meter_levels_observed"] == false);
    CHECK(
        compiled["postconditions"]["track_gates"][0]["input_stereo_momentary_quiescent_verified"] ==
        false);
    CHECK(compiled["postconditions"]["track_gates"][0]
                  ["output_stereo_momentary_quiescent_verified"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["meter_momentary_quiescence_verified"] ==
          false);
    CHECK(compiled["postconditions"]["track_gates"][0]["meter_quiescence_verified"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["continuous_input_silence_verified"] ==
          false);
    CHECK(compiled["postconditions"]["track_gates"][0]["continuous_output_silence_verified"] ==
          false);
    const auto& track_input_routing = compiled["postconditions"]["track_gates"][0]["input_routing"];
    CHECK(track_input_routing["selected_type_display_name"].is_null());
    CHECK(track_input_routing["selected_channel_identifier"].is_null());
    CHECK(track_input_routing["available_types"].empty());
    CHECK(track_input_routing["available_channels"].empty());
    CHECK(track_input_routing["selected_input_observed"] == false);
    CHECK(track_input_routing["selected_input_available_verified"] == false);
    CHECK(track_input_routing["input_source_identity_mapped"] == false);
    CHECK(track_input_routing["external_input_neutrality_verified"] == false);
    const auto& track_output_routing =
        compiled["postconditions"]["track_gates"][0]["output_routing"];
    CHECK(track_output_routing["requested_destination"] == "master");
    CHECK(track_output_routing["requested_group_id"].is_null());
    CHECK(track_output_routing["selected_type_display_name"].is_null());
    CHECK(track_output_routing["selected_type_identifier"].is_null());
    CHECK(track_output_routing["selected_channel_display_name"].is_null());
    CHECK(track_output_routing["selected_channel_identifier"].is_null());
    CHECK(track_output_routing["available_types"].empty());
    CHECK(track_output_routing["available_channels"].empty());
    CHECK(track_output_routing["selected_type_available_verified"] == false);
    CHECK(track_output_routing["selected_channel_available_verified"] == false);
    CHECK(track_output_routing["selected_output_available_verified"] == false);
    CHECK(track_output_routing["selected_output_observed"] == false);
    CHECK(track_output_routing["source_target_identity_mapped"] == false);
    CHECK(track_output_routing["verified"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_is_frozen"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["unfrozen_verified"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_arm"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_arm"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["disarmed_verified"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_implicit_arm"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_implicit_arm"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["implicitly_disarmed_verified"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_back_to_arranger"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_fired_slot_index"] == -1);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_playing_slot_index"] == -1);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_clip_slot_is_group_slot"] ==
          false);
    CHECK(
        compiled["postconditions"]["track_gates"][0]["requested_clip_slot_controls_other_clips"] ==
        false);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_clip_slot_is_playing"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_clip_slot_is_recording"] ==
          false);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_clip_slot_is_triggered"] ==
          false);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_clip_slot_playing_status"] == 0);
    CHECK(
        compiled["postconditions"]["track_gates"][0]["requested_clip_slot_will_record_on_start"] ==
        false);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_arrangement_clip_count"] == 0);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_take_lane_count"] == 0);
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_back_to_arranger"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_fired_slot_index"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_playing_slot_index"].is_null());
    CHECK(
        compiled["postconditions"]["track_gates"][0]["observed_arrangement_clip_count"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_take_lane_count"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_clip_slots"].empty());
    CHECK(compiled["postconditions"]["track_gates"][0]["clip_slot_states_observed"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["clip_slot_non_group_semantics_verified"] ==
          false);
    CHECK(compiled["postconditions"]["track_gates"][0]["clip_slot_playback_idle_verified"] ==
          false);
    CHECK(compiled["postconditions"]["track_gates"][0]["clip_slot_recording_quiescence_verified"] ==
          false);
    CHECK(compiled["postconditions"]["track_gates"][0]["clip_slot_launch_quiescence_verified"] ==
          false);
    CHECK(compiled["postconditions"]["track_gates"][0]["clip_slot_runtime_quiescence_verified"] ==
          false);
    CHECK(compiled["postconditions"]["track_gates"][0]["arrangement_clip_topology_observed"] ==
          false);
    CHECK(compiled["postconditions"]["track_gates"][0]["arrangement_content_absent_verified"] ==
          false);
    CHECK(compiled["postconditions"]["track_gates"][0]["take_lane_topology_observed"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["take_lanes_absent_verified"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["session_launch_quiescent_verified"] ==
          false);
    CHECK(compiled["postconditions"]["track_gates"][0]
                  ["track_arrangement_playback_aligned_verified"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["requested_group_track_index"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["group_membership_observed"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["observed_group_track_index"].is_null());
    CHECK(compiled["postconditions"]["track_gates"][0]["ungrouped_verified"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]["monitoring_state_observed"] == false);
    CHECK(compiled["postconditions"]["track_gates"][0]
                  ["clip_output_not_suppressed_by_monitoring_verified"] == false);
    CHECK(compiled["postconditions"]["return_track_gates_requested"] == 1);
    CHECK(compiled["postconditions"]["return_track_gates_verified"] == 0);
    REQUIRE(compiled["postconditions"]["return_track_gates"].size() == 1);
    CHECK(compiled["postconditions"]["return_track_gates"][0]["aux_bus_id"] == aux["aux_bus_id"]);
    CHECK(compiled["postconditions"]["return_track_gates"][0]["track_index"] == 0);
    CHECK(compiled["postconditions"]["return_track_gates"][0]["requested_name"] == "Hall");
    CHECK(compiled["postconditions"]["return_track_gates"][0]["expected_mixer_enabled"] == true);
    CHECK(compiled["postconditions"]["return_track_gates"][0]["observed_name"].is_null());
    const auto& return_output_routing =
        compiled["postconditions"]["return_track_gates"][0]["output_routing"];
    CHECK(return_output_routing["requested_destination"] == "master");
    CHECK(return_output_routing["requested_group_id"].is_null());
    CHECK(return_output_routing["selected_type_identifier"].is_null());
    CHECK(return_output_routing["available_types"].empty());
    CHECK(return_output_routing["available_channels"].empty());
    CHECK(return_output_routing["selected_type_available_verified"] == false);
    CHECK(return_output_routing["selected_channel_available_verified"] == false);
    CHECK(return_output_routing["selected_output_available_verified"] == false);
    CHECK(return_output_routing["selected_output_observed"] == false);
    CHECK(return_output_routing["source_target_identity_mapped"] == false);
    CHECK(return_output_routing["verified"] == false);
    CHECK(compiled["postconditions"]["return_track_gates"][0]["observed_mute"].is_null());
    CHECK(compiled["postconditions"]["return_track_gates"][0]["observed_solo"].is_null());
    CHECK(compiled["postconditions"]["return_track_gates"][0]["observed_muted_via_solo"].is_null());
    CHECK(
        compiled["postconditions"]["return_track_gates"][0]["observed_crossfade_assign"].is_null());
    CHECK(compiled["postconditions"]["return_track_gates"][0]["observed_panning_mode"].is_null());
    CHECK(compiled["postconditions"]["return_track_gates"][0]["observed_track_activator_minimum"]
              .is_null());
    CHECK(
        compiled["postconditions"]["return_track_gates"][0]["observed_track_activator_is_quantized"]
            .is_null());
    CHECK(compiled["postconditions"]["return_track_gates"][0]
                  ["track_activator_quantization_verified"] == false);
    CHECK(compiled["postconditions"]["return_track_gates"][0]["verified"] == false);
    CHECK(compiled["postconditions"]["master_track_gates_requested"] == 1);
    CHECK(compiled["postconditions"]["master_track_gates_verified"] == 0);
    CHECK(compiled["postconditions"]["master_track_gate"]["requested_track_activator"] == 1.0);
    CHECK(compiled["postconditions"]["master_track_gate"]["requested_panning_mode"] == 0);
    CHECK(compiled["postconditions"]["master_track_gate"]["requested_pan"] == 0.0);
    CHECK(compiled["postconditions"]["master_track_gate"]["observed_pan"].is_null());
    CHECK(compiled["postconditions"]["master_track_gate"]["observed_pan_minimum"].is_null());
    CHECK(compiled["postconditions"]["master_track_gate"]["observed_pan_is_quantized"].is_null());
    CHECK(compiled["postconditions"]["master_track_gate"]["pan_range_verified"] == false);
    CHECK(compiled["postconditions"]["master_track_gate"]["pan_quantization_verified"] == false);
    CHECK(compiled["postconditions"]["master_track_gate"]["verified"] == false);
    CHECK(compiled["postconditions"]["devices_requested"] == 2);
    CHECK(compiled["postconditions"]["devices_verified"] == 0);
    REQUIRE(compiled["postconditions"]["devices"].size() == 2);
    CHECK(compiled["postconditions"]["devices"][0]["requested_type"] == 1);
    CHECK(compiled["postconditions"]["devices"][0]["expected_chain_size"] == 1);
    CHECK(compiled["postconditions"]["devices"][0]["observed_active"].is_null());
    CHECK(compiled["postconditions"]["devices"][0]["observed_can_have_chains"].is_null());
    CHECK(compiled["postconditions"]["devices"][0]["observed_latency_in_samples"].is_null());
    CHECK(compiled["postconditions"]["devices"][0]["observed_latency_in_ms"].is_null());
    CHECK(compiled["postconditions"]["devices"][0]["reported_latency_observed"] == false);
    CHECK(compiled["postconditions"]["devices"][0]["flat_device_verified"] == false);
    CHECK(compiled["postconditions"]["devices"][0]["render_path_latency_fully_observed"] == false);
    CHECK(compiled["postconditions"]["clips_requested"] == 2);
    CHECK(compiled["postconditions"]["clips_verified"] == 0);
    REQUIRE(compiled["postconditions"]["clips"].size() == 2);
    CHECK(compiled["postconditions"]["clips"][0]["requested_length"] == 4.0);
    CHECK(compiled["postconditions"]["clips"][0]["requested_occupied_slot_indices"] ==
          nlohmann::json::array({0}));
    CHECK(compiled["postconditions"]["clips"][0]["observed_occupied_slot_indices"].empty());
    CHECK(compiled["postconditions"]["clips"][0]["exact_slot_occupancy_verified"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["groove_state_available"] == true);
    CHECK(compiled["postconditions"]["clips"][0]["requested_has_groove"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["launch_state_available"] == true);
    CHECK(compiled["postconditions"]["clips"][0]["requested_launch_mode"] == 0);
    CHECK(compiled["postconditions"]["clips"][0]["requested_is_audio_clip"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["requested_is_midi_clip"] == true);
    CHECK(compiled["postconditions"]["clips"][0]["requested_is_arrangement_clip"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["location_identity_available"] == true);
    CHECK(compiled["postconditions"]["clips"][0]["requested_is_session_clip"] == true);
    CHECK(compiled["postconditions"]["clips"][0]["requested_is_take_lane_clip"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["requested_end_time"] == 4.0);
    CHECK(compiled["postconditions"]["clips"][0]["requested_launch_quantization"] == 1);
    CHECK(compiled["postconditions"]["clips"][0]["requested_legato"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["requested_velocity_amount"] == 0.0);
    CHECK(compiled["postconditions"]["clips"][0]["observed_length"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["observed_is_audio_clip"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["observed_is_midi_clip"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["observed_is_arrangement_clip"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["observed_is_session_clip"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["observed_is_take_lane_clip"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["clip_identity_verified"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["observed_end_time"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["playback_end_verified"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["observed_has_groove"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["groove_absence_verified"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["requested_has_envelopes"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["observed_has_envelopes"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["requested_is_playing"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["requested_is_recording"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["requested_is_overdubbing"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["requested_is_triggered"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["requested_will_record_on_start"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["observed_is_playing"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["observed_is_recording"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["observed_is_overdubbing"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["observed_is_triggered"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["observed_will_record_on_start"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["runtime_state_observed"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["recording_quiescence_verified"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["playback_idle_verified"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["mpe_note_expression_state_observed"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["mpe_note_expression_neutrality_verified"] ==
          false);
    CHECK(compiled["postconditions"]["clips"][0]["envelope_absence_verified"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["observed_launch_mode"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["observed_launch_quantization"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["observed_legato"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["observed_velocity_amount"].is_null());
    CHECK(compiled["postconditions"]["clips"][0]["launch_behavior_verified"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["follow_actions_observed"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["one_shot_playback_verified"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["midi_bank_program_state_observed"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["program_change_suppression_verified"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["audible_timing_verified"] == false);
    CHECK(compiled["postconditions"]["clips"][0]["audible_velocity_verified"] == false);
    CHECK(compiled["postconditions"]["note_batches_requested"] == 1);
    CHECK(compiled["postconditions"]["note_batches_verified"] == 0);
    CHECK(compiled["postconditions"]["notes_requested"] == 1);
    CHECK(compiled["postconditions"]["notes_verified"] == 0);
    REQUIRE(compiled["postconditions"]["note_batches"].size() == 1);
    CHECK(compiled["postconditions"]["note_batches"][0]["deployment_action"] == "recorded_only");
    CHECK(compiled["postconditions"]["note_batches"][0]["requested_notes"][0]["pitch"] == 60);
    CHECK(compiled["postconditions"]["note_batches"][0]["requested_note_ids"].empty());
    CHECK(compiled["postconditions"]["note_batches"][0]["observed_notes"].empty());
    CHECK(compiled["postconditions"]["cues_requested"] == 1);
    CHECK(compiled["postconditions"]["cues_verified"] == 0);
    REQUIRE(compiled["postconditions"]["cues"].size() == 1);
    CHECK(compiled["postconditions"]["cues"][0]["requested_name"] == "A");
    CHECK(compiled["postconditions"]["cues"][0]["observed_name"].is_null());
    CHECK(compiled["postconditions"]["mixer_properties_requested"] == 12);
    CHECK(compiled["postconditions"]["mixer_properties_verified"] == 0);
    REQUIRE(compiled["postconditions"]["mixer_properties"].size() == 12);
    CHECK(compiled["postconditions"]["mixer_properties"][0]["parameter_path"] ==
          "song/master_track/mixer_device/panning");
    CHECK(compiled["postconditions"]["mixer_properties"][0]["requested_is_quantized"] == false);
    CHECK(compiled["postconditions"]["mixer_properties"][0]["observed_value"].is_null());
    CHECK(compiled["postconditions"]["mixer_properties"][0]["observed_minimum"].is_null());
    CHECK(compiled["postconditions"]["mixer_properties"][0]["observed_maximum"].is_null());
    CHECK(compiled["postconditions"]["mixer_properties"][0]["observed_is_quantized"].is_null());
    CHECK(compiled["postconditions"]["mixer_properties"][0]["observed_default_value"].is_null());
    CHECK(compiled["postconditions"]["mixer_properties"][0]["observed_value_items"].is_null());
    CHECK(compiled["postconditions"]["mixer_properties"][0]["range_verified"].is_null());
    CHECK(compiled["postconditions"]["mixer_properties"][0]["quantization_verified"] == false);
    CHECK(compiled["postconditions"]["device_parameters_requested"] == 1);
    CHECK(compiled["postconditions"]["device_parameters_verified"] == 0);
    REQUIRE(compiled["postconditions"]["device_parameters"].size() == 1);
    const auto& parameter = compiled["postconditions"]["device_parameters"][0];
    CHECK(parameter["origin"] == "timbre");
    CHECK(parameter["part_id"] == first_part);
    CHECK(parameter["track_index"] == 0);
    CHECK(parameter["source_path"] == "source.filter.cutoff");
    CHECK(parameter["effect_id"].is_null());
    CHECK(parameter["device_path"] == "song/tracks/0/devices/0");
    CHECK(parameter["requested_name"] == "Filter Freq");
    CHECK(parameter["value_property"] == "value");
    CHECK(parameter["range_min"] == 0.0);
    CHECK(parameter["range_max"] == 1.0);
    CHECK(parameter["deployment_action"] == "recorded_only");
    CHECK(parameter["matched_name"].is_null());
    CHECK(parameter["original_name"].is_null());
    CHECK(parameter["observed_value"].is_null());
    CHECK(parameter["observed_minimum"].is_null());
    CHECK(parameter["observed_maximum"].is_null());
    CHECK(parameter["observed_is_quantized"].is_null());
    CHECK(parameter["observed_default_value"].is_null());
    CHECK(parameter["observed_value_items"].is_null());
    CHECK(parameter["observed_is_enabled"].is_null());
    CHECK(parameter["observed_state"].is_null());
    CHECK(parameter["observed_automation_state"].is_null());
    CHECK(parameter["identity_verified"] == false);
    CHECK(parameter["value_verified"] == false);
    CHECK(parameter["range_verified"].is_null());
    CHECK(parameter["enabled_verified"] == false);
    CHECK(parameter["state_verified"] == false);
    CHECK(parameter["automation_verified"] == false);
    CHECK(parameter["verified"] == false);
    CHECK(compiled["part_tracks"][0]["part_id"] == first_part);
    CHECK(compiled["part_tracks"][0]["track_index"] == 0);
    CHECK(compiled["part_tracks"][1]["part_id"] == second_part);
    CHECK(compiled["part_tracks"][1]["track_index"] == 1);
    CHECK(compiled["deployment"]["status"] == "completed");
    CHECK(compiled["deployment"]["target_may_be_partially_modified"] == false);
    CHECK(compiled["deployment"]["mutation_journal"].size() ==
          compiled["plan"]["planned_mutations"].size());

    bool part_two_fader_targets_track_one = false;
    for (const auto* entry : transport.find_by_type(LomRequestType::SetProperty)) {
        if (entry->request.path.to_string() == "song/tracks/1/mixer_device/volume" &&
            entry->request.property_or_method == "display_value" &&
            std::get<double>(entry->request.args.at(0)) == -12.0)
            part_two_fader_targets_track_one = true;
    }
    CHECK(part_two_fader_targets_track_one);
}

TEST_CASE("Project MCP plan/apply is read-only until one guarded one-shot application",
          "[mcp][integration][project][plan]") {
    CommandBuffer transport;
    McpServer server;
    McpSession session;
    register_score_tools(server, &transport, session.score);
    register_timbre_tools(server, &transport, session.timbre);
    register_mix_tools(server, &transport, session.mix);
    register_project_tools(server, session, &transport);

    const auto score = call_tool(server,
                                 "score_create",
                                 {{"title", "Guarded"},
                                  {"total_bars", 1},
                                  {"parts", {{{"name", "One"}, {"instrument_type", 0}}}}},
                                 570);
    const auto profile = call_tool(
        server, "create_timbre_profile", {{"part_id", score["part_ids"][0]}, {"name", "One"}}, 571);
    const auto mix = call_tool(server, "create_mix_graph", {{"part_ids", score["part_ids"]}}, 572);
    const json project_ids = {
        {"score_id", score["score_id"]},
        {"timbre_profile_ids", {profile["profile_id"]}},
        {"mix_graph_id", mix["graph_id"]},
        {"output_routing_bindings",
         {{"part_tracks",
           {{{"part_id", score["part_ids"][0]},
             {"type", {{"display_name", "Master"}, {"identifier", "master"}}},
             {"channel", {{"display_name", "1/2"}, {"identifier", "stereo_1_2"}}},
             {"mapping_provenance", "guarded plan fixture"}}}},
          {"aux_returns", json::array()}}}};

    const auto plan = call_tool(server, "project_plan_to_ableton", project_ids, 573);
    REQUIRE(plan["success"] == true);
    CHECK(plan["one_shot"] == true);
    CHECK_FALSE(plan["planned_mutations"].empty());
    CHECK(plan["output_routing_bindings"] == project_ids["output_routing_bindings"]);
    CHECK(std::ranges::count_if(plan["planned_mutations"], [](const auto& mutation) {
              return mutation["request"]["name"].template get<std::string>().starts_with(
                  "sunny_set_output_routing_");
          }) == 2);
    CHECK(transport.entries().empty());

    const auto applied =
        call_tool(server, "project_apply_ableton_plan", {{"plan_id", plan["plan_id"]}}, 574);
    REQUIRE(applied["success"] == true);
    CHECK(applied["plan_consumed"] == true);
    CHECK(applied["deployment"]["status"] == "completed");
    CHECK(applied["mix"]["output_routing_bindings"] == project_ids["output_routing_bindings"]);
    CHECK(applied["mix"]["output_routes_written"] == 1);
    CHECK(applied["mix"]["output_route_deployments"][0]["action"] == "recorded_only");
    CHECK(applied["deployment"]["mutation_journal"].size() == plan["planned_mutations"].size());
    const auto entries_after_apply = transport.entries().size();

    const auto repeated =
        call_tool(server, "project_apply_ableton_plan", {{"plan_id", plan["plan_id"]}}, 575);
    CHECK(repeated["success"] == false);
    CHECK(repeated["plan_consumed"] == true);
    CHECK(repeated["error_code"] == static_cast<int>(sunny::core::ErrorCode::ProjectPlanConsumed));
    CHECK(transport.entries().size() == entries_after_apply);
}

TEST_CASE("Project MCP immediate compilation honors and validates explicit output routes",
          "[mcp][integration][project][routing]") {
    CommandBuffer transport;
    McpServer server;
    McpSession session;
    register_score_tools(server, &transport, session.score);
    register_timbre_tools(server, &transport, session.timbre);
    register_mix_tools(server, &transport, session.mix);
    register_project_tools(server, session, &transport);

    const auto score =
        call_tool(server,
                  "score_create",
                  {{"total_bars", 1}, {"parts", {{{"name", "One"}, {"instrument_type", 0}}}}},
                  5760);
    const auto profile = call_tool(server,
                                   "create_timbre_profile",
                                   {{"part_id", score["part_ids"][0]}, {"name", "One"}},
                                   5761);
    const auto mix = call_tool(server, "create_mix_graph", {{"part_ids", score["part_ids"]}}, 5762);
    const auto aux = call_tool(
        server, "create_aux_bus", {{"graph_id", mix["graph_id"]}, {"name", "Hall"}}, 5763);
    json bindings = {{"part_tracks",
                      {{{"part_id", score["part_ids"][0]},
                        {"type", {{"display_name", "Master"}, {"identifier", "master"}}},
                        {"channel", {{"display_name", "1/2"}, {"identifier", "stereo_1_2"}}},
                        {"mapping_provenance", "immediate part route fixture"}}}},
                     {"aux_returns",
                      {{{"aux_bus_id", aux["aux_bus_id"]},
                        {"type", {{"display_name", "Master"}, {"identifier", "master"}}},
                        {"channel", {{"display_name", "1/2"}, {"identifier", "stereo_1_2"}}},
                        {"mapping_provenance", "immediate return route fixture"}}}}};

    bool valid = false;
    SECTION("Part and Aux bindings reach mutations and retained deployment evidence") {
        valid = true;
    }
    SECTION("Malformed container is rejected before mutation") {
        bindings["part_tracks"] = json::object();
    }
    SECTION("Incomplete routing dictionary is rejected before mutation") {
        bindings["part_tracks"][0]["type"].erase("identifier");
    }
    SECTION("Empty provenance is rejected before mutation") {
        bindings["aux_returns"][0]["mapping_provenance"] = "";
    }
    SECTION("Duplicate Part bindings are rejected before mutation") {
        const auto duplicate = bindings["part_tracks"][0];
        bindings["part_tracks"].push_back(duplicate);
    }
    SECTION("Duplicate Aux bindings are rejected before mutation") {
        const auto duplicate = bindings["aux_returns"][0];
        bindings["aux_returns"].push_back(duplicate);
    }

    const auto compiled = call_tool(server,
                                    "project_compile_to_ableton",
                                    {{"score_id", score["score_id"]},
                                     {"timbre_profile_ids", {profile["profile_id"]}},
                                     {"mix_graph_id", mix["graph_id"]},
                                     {"output_routing_bindings", bindings}},
                                    5764);
    if (!valid) {
        CHECK(compiled.contains("error"));
        CHECK_FALSE(compiled.value("success", false));
        CHECK(transport.entries().empty());
        return;
    }

    REQUIRE(compiled["success"] == true);
    REQUIRE(compiled["mix"]["output_routing_bindings"] == bindings);
    CHECK(compiled["mix"]["output_routes_requested"] == 2);
    CHECK(compiled["mix"]["output_routes_written"] == 2);
    CHECK(compiled["mix"]["output_routes_verified"] == 0);
    const auto& deployments = compiled["mix"]["output_route_deployments"];
    REQUIRE(deployments.size() == 2);
    CHECK(deployments[0]["part_id"] == score["part_ids"][0]);
    CHECK(deployments[0]["binding"]["mapping_provenance"] == "immediate part route fixture");
    CHECK(deployments[1]["aux_bus_id"] == aux["aux_bus_id"]);
    CHECK(deployments[1]["binding"]["mapping_provenance"] == "immediate return route fixture");
    for (const auto& deployment : deployments) {
        CHECK(deployment["action"] == "recorded_only");
        CHECK(deployment["verified"] == false);
    }

    std::set<std::string> route_commands;
    for (const auto& entry : transport.entries()) {
        const auto& request = entry.request;
        if (!request.property_or_method.starts_with("sunny_set_output_routing_")) continue;
        const bool type_stage = request.property_or_method.ends_with("_type");
        const auto key = type_stage ? "type" : "channel";
        CHECK(request.property_or_method == std::string("sunny_set_output_routing_") + key);
        REQUIRE(request.args.size() == (type_stage ? 1 : 2));
        CHECK(std::get<json>(request.args[0]) == bindings["part_tracks"][0]["type"]);
        if (!type_stage)
            CHECK(std::get<json>(request.args[1]) == bindings["part_tracks"][0]["channel"]);
        CHECK(route_commands.insert(request.path.to_string() + "/" + key).second);
    }
    CHECK(route_commands == std::set<std::string>{"song/tracks/0/type",
                                                  "song/tracks/0/channel",
                                                  "song/return_tracks/0/type",
                                                  "song/return_tracks/0/channel"});
    CHECK(std::ranges::count_if(compiled["plan"]["planned_mutations"], [](const auto& mutation) {
              return mutation["request"]["name"].template get<std::string>().starts_with(
                  "sunny_set_output_routing_");
          }) == 4);
    CHECK(compiled["deployment"]["mutation_journal"].size() ==
          compiled["plan"]["planned_mutations"].size());
}

TEST_CASE("Project MCP rejects invalid correspondence before Live mutation",
          "[mcp][integration][project][preflight]") {
    CommandBuffer transport;
    McpServer server;
    McpSession session;
    register_score_tools(server, &transport, session.score);
    register_mix_tools(server, &transport, session.mix);
    register_project_tools(server, session, &transport);

    const auto score =
        call_tool(server,
                  "score_create",
                  {{"total_bars", 1}, {"parts", {{{"name", "One"}, {"instrument_type", 0}}}}},
                  567);
    const auto mix = call_tool(server, "create_mix_graph", {{"part_ids", score["part_ids"]}}, 568);
    const auto compiled = call_tool(server,
                                    "project_compile_to_ableton",
                                    {{"score_id", score["score_id"]},
                                     {"timbre_profile_ids", json::array()},
                                     {"mix_graph_id", mix["graph_id"]}},
                                    569);

    CHECK(compiled["success"] == false);
    CHECK(compiled["error_code"] ==
          static_cast<int>(sunny::core::ErrorCode::ProjectValidationFailed));
    CHECK(transport.entries().empty());
}

TEST_CASE("Ableton MCP results distinguish requested and written automation",
          "[mcp][integration][ableton][automation]") {
    CommandBuffer transport;
    McpServer server;
    register_timbre_tools(server, &transport);
    register_mix_tools(server, &transport);

    const auto profile =
        call_tool(server, "create_timbre_profile", {{"part_id", 1}, {"name", "Automated"}}, 600);
    REQUIRE(call_tool(server,
                      "add_automation",
                      {{"profile_id", profile["profile_id"]},
                       {"path", "source.filter.cutoff"},
                       {"breakpoints",
                        {{{"bar", 1}, {"value", 2000.0}}, {{"bar", 2}, {"value", 8000.0}}}}},
                      601)["success"] == true);
    const auto timbre = call_tool(
        server, "compile_timbre", {{"profile_id", profile["profile_id"]}, {"track_index", 0}}, 602);
    REQUIRE(timbre["success"] == true);
    CHECK(timbre["automation_lanes_requested"] == 1);
    CHECK(timbre["automation_lanes_written"] == 0);
    CHECK_FALSE(timbre.contains("automation_lanes"));
    CHECK(timbre["complete"] == false);

    const auto graph = call_tool(server, "create_mix_graph", {{"part_ids", {1}}}, 603);
    REQUIRE(call_tool(
                server,
                "add_mix_automation",
                {{"graph_id", graph["graph_id"]},
                 {"target", "channels[1].fader.level_db"},
                 {"breakpoints", {{{"bar", 1}, {"value", -12.0}}, {{"bar", 2}, {"value", -6.0}}}}},
                604)["success"] == true);
    const auto mix =
        call_tool(server, "compile_mix", {{"graph_id", graph["graph_id"]}, {"base_track", 0}}, 605);
    REQUIRE(mix["success"] == true);
    CHECK(mix["automation_lanes_requested"] == 1);
    CHECK(mix["automation_lanes_written"] == 0);
    CHECK_FALSE(mix.contains("automation_lanes"));
    CHECK(mix["complete"] == false);
}

TEST_CASE("Timbre MCP authors every owned modulation source and complete routing payload",
          "[mcp][integration][timbre][modulation]") {
    McpServer server;
    register_timbre_tools(server);
    const auto profile =
        call_tool(server, "create_timbre_profile", {{"part_id", 1}, {"name", "Modulated"}}, 610);

    const auto lfo =
        call_tool(server,
                  "create_modulation_lfo",
                  {{"profile_id", profile["profile_id"]},
                   {"waveform", static_cast<int>(sunny::core::LFOWaveformType::Custom)},
                   {"custom_points", {{{"x", 0.0}, {"y", -1.0}}, {{"x", 1.0}, {"y", 1.0}}}}},
                  611);
    REQUIRE(lfo["success"] == true);
    CHECK(lfo["source_index"] == 0);

    const auto envelope = call_tool(server,
                                    "create_modulation_envelope",
                                    {{"profile_id", profile["profile_id"]},
                                     {"stages",
                                      {{{"duration", 10.0}, {"target_level", 1.0}},
                                       {{"duration", 100.0}, {"target_level", 0.5}},
                                       {{"duration", 200.0}, {"target_level", 0.0}}}}},
                                    612);
    REQUIRE(envelope["success"] == true);
    CHECK(envelope["source_index"] == 0);

    const auto sequencer = call_tool(server,
                                     "create_step_sequencer",
                                     {{"profile_id", profile["profile_id"]},
                                      {"steps", {0.0, 0.5, 1.0}},
                                      {"duration_num", 1},
                                      {"duration_den", 16}},
                                     613);
    REQUIRE(sequencer["success"] == true);
    CHECK(sequencer["source_index"] == 0);

    const auto routing =
        call_tool(server,
                  "add_modulation",
                  {{"profile_id", profile["profile_id"]},
                   {"source_type", static_cast<int>(sunny::core::ModulationSourceType::Envelope)},
                   {"source_index", 0},
                   {"via_type", static_cast<int>(sunny::core::ModulationSourceType::LFO)},
                   {"via_index", 0},
                   {"target", "source.filter.cutoff"},
                   {"depth", 0.5}},
                  614);
    CHECK(routing["success"] == true);

    const auto validation =
        call_tool(server, "validate_timbre", {{"profile_id", profile["profile_id"]}}, 615);
    CHECK(validation["valid"] == true);

    const auto invalid_beat = call_tool(
        server,
        "add_automation",
        {{"profile_id", profile["profile_id"]},
         {"path", "source.filter.cutoff"},
         {"breakpoints", {{{"bar", 1}, {"beat_num", 0}, {"beat_den", 0}, {"value", 1000.0}}}}},
        616);
    CHECK(invalid_beat.contains("error"));
}

TEST_CASE("IR deployment tools remain discoverable and decline while offline",
          "[mcp][integration][offline]") {
    McpServer server;
    register_score_tools(server);
    register_timbre_tools(server);
    register_mix_tools(server);
    McpSession session;
    register_project_tools(server, session, nullptr);

    auto tools_response =
        server.process_request({{"jsonrpc", "2.0"}, {"method", "tools/list"}, {"id", 56}});
    std::set<std::string> names;
    for (const auto& tool : tools_response["result"]["tools"])
        names.insert(tool["name"].get<std::string>());
    CHECK(names.contains("score_compile_to_ableton"));
    CHECK(names.contains("score_set_tuning"));
    CHECK(names.contains("compile_timbre"));
    CHECK(names.contains("compile_mix"));
    CHECK(names.contains("map_mix_effect_parameter"));
    CHECK(names.contains("set_channel_relative_level"));
    CHECK(names.contains("resolve_mix_fader_levels"));
    CHECK(names.contains("project_plan_to_ableton"));
    CHECK(names.contains("project_apply_ableton_plan"));

    auto created =
        call_tool(server,
                  "score_create",
                  {{"total_bars", 1}, {"parts", {{{"name", "Piano"}, {"instrument_type", 0}}}}},
                  57);
    auto declined =
        call_tool(server, "score_compile_to_ableton", {{"score_id", created["score_id"]}}, 58);
    CHECK(declined["success"] == false);
    CHECK(declined["connected"] == false);
    CHECK(declined["error"] == "Ableton transport unavailable");
}

TEST_CASE("MCP discriminators decline unknown IR variants", "[mcp][validation]") {
    McpServer timbre_server;
    register_timbre_tools(timbre_server);
    auto profile =
        call_tool(timbre_server, "create_timbre_profile", {{"part_id", 1}, {"name", "Lead"}}, 60);

    auto bad_source =
        call_tool(timbre_server,
                  "set_sound_source",
                  {{"profile_id", profile["profile_id"]}, {"source_type", "imaginary_synth"}},
                  61);
    CHECK(bad_source["error"] == "Unknown or invalid sound source configuration");

    auto bad_timbre_effect =
        call_tool(timbre_server,
                  "add_effect",
                  {{"profile_id", profile["profile_id"]}, {"effect_type", "imaginary_effect"}},
                  62);
    CHECK(bad_timbre_effect["error"] == "Unknown or invalid timbre effect configuration");

    McpServer mix_server;
    register_mix_tools(mix_server);
    auto graph = call_tool(mix_server, "create_mix_graph", {{"part_ids", {1}}}, 63);
    auto bad_mix_effect =
        call_tool(mix_server,
                  "add_master_effect",
                  {{"graph_id", graph["graph_id"]}, {"effect_type", "imaginary_effect"}},
                  64);
    CHECK(bad_mix_effect["error"] == "Unknown or invalid mix effect configuration");
}

TEST_CASE("Mix MCP authors first-class reverb and tempo-synchronised delay returns",
          "[mcp][integration][mix][effects]") {
    McpServer server;
    register_mix_tools(server);

    auto graph = call_tool(server, "create_mix_graph", {{"part_ids", {1}}}, 65);
    auto aux = call_tool(
        server, "create_aux_bus", {{"graph_id", graph["graph_id"]}, {"name", "Spatial"}}, 66);
    auto reverb = call_tool(server,
                            "add_aux_effect",
                            {{"graph_id", graph["graph_id"]},
                             {"aux_id", aux["aux_bus_id"]},
                             {"effect_type", "reverb"},
                             {"reverb_algorithm", 5},
                             {"decay_time", 3.2},
                             {"mix", 1.0}},
                            67);
    auto delay = call_tool(server,
                           "add_aux_effect",
                           {{"graph_id", graph["graph_id"]},
                            {"aux_id", aux["aux_bus_id"]},
                            {"effect_type", "delay"},
                            {"tempo_synced", true},
                            {"beat_numerator", 3},
                            {"beat_denominator", 16},
                            {"stereo_mode", 2}},
                           68);

    CHECK(reverb["effect_id"] == 1);
    CHECK(delay["effect_id"] == 2);
    auto stored = call_tool(server, "get_mix_json", {{"graph_id", graph["graph_id"]}}, 69);
    const auto& effects = stored["mix_ir"]["aux_buses"][0]["effect_chain"];
    REQUIRE(effects.size() == 2);
    CHECK(effects[0]["type"] == "reverb");
    CHECK(effects[0]["params"]["decay_time"] == 3.2f);
    CHECK(effects[1]["type"] == "delay");
    CHECK(effects[1]["params"]["beat_division"]["n"] == 3);
    CHECK(effects[1]["params"]["beat_division"]["d"] == 16);

    auto rejected = call_tool(server,
                              "add_aux_effect",
                              {{"graph_id", graph["graph_id"]},
                               {"aux_id", aux["aux_bus_id"]},
                               {"effect_type", "reverb"},
                               {"decay_time", -1.0}},
                              70);
    CHECK(rejected["error"] == "Unknown or invalid mix effect configuration");
    auto after_rejection = call_tool(server, "get_mix_json", {{"graph_id", graph["graph_id"]}}, 71);
    CHECK(after_rejection["mix_ir"]["aux_buses"][0]["effect_chain"].size() == 2);
}

TEST_CASE("Score MCP rejects incomplete nested values instead of inventing defaults",
          "[mcp][validation][score]") {
    McpServer server;
    register_score_tools(server);

    auto bad_part = call_tool(server, "score_create", {{"parts", {{{"name", "Piano"}}}}}, 70);
    CHECK(bad_part["error"] == "each part requires name and instrument_type");

    auto created =
        call_tool(server,
                  "score_create",
                  {{"total_bars", 2}, {"parts", {{{"name", "Piano"}, {"instrument_type", 0}}}}},
                  71);
    REQUIRE(created.contains("score_id"));

    auto bad_plan = call_tool(
        server,
        "score_set_formal_plan",
        {{"score_id", created["score_id"]}, {"sections", {{{"label", "A"}, {"end_bar", 2}}}}},
        72);
    CHECK(bad_plan["error"] == "each section requires label, start_bar, and end_bar");

    auto bad_pitch = call_tool(server,
                               "score_insert_note",
                               {{"score_id", created["score_id"]},
                                {"part_id", 1},
                                {"bar", 1},
                                {"pitch", {{"letter", "C"}, {"accidental", 0}}},
                                {"duration", {{"n", 1}, {"d", 4}}}},
                               73);
    CHECK(bad_pitch["error"] == "invalid pitch or duration");

    auto bad_view = call_tool(server,
                              "score_get_reduction",
                              {{"score_id", created["score_id"]}, {"view_type", "invented-view"}},
                              74);
    CHECK(bad_view["error"] == "get_reduction failed: invalid view or region");

    auto bad_region = call_tool(
        server,
        "score_get_reduction",
        {{"score_id", created["score_id"]}, {"region", {{"start_bar", 2}, {"end_bar", 1}}}},
        75);
    CHECK(bad_region["error"] == "invalid region");
}

TEST_CASE("Score MCP rejects integer narrowing before mutation",
          "[mcp][validation][score][transaction]") {
    McpServer server;
    register_score_tools(server);

    auto invalid_create = call_tool(server, "score_create", {{"key_accidentals", 256}}, 76);
    CHECK(invalid_create.contains("error"));

    auto created =
        call_tool(server,
                  "score_create",
                  {{"total_bars", 1}, {"parts", {{{"name", "Piano"}, {"instrument_type", 0}}}}},
                  77);
    REQUIRE(created["score_id"] == 1);

    auto invalid_voice = call_tool(server,
                                   "score_insert_note",
                                   {{"score_id", 1},
                                    {"part_id", 1},
                                    {"bar", 1},
                                    {"voice", 256},
                                    {"pitch", {{"letter", "C"}, {"accidental", 0}, {"octave", 4}}},
                                    {"duration", {{"n", 1}, {"d", 4}}}},
                                   78);
    CHECK(invalid_voice.contains("error"));

    auto invalid_ppq = call_tool(
        server, "score_compile_to_midi", {{"score_id", 1}, {"ppq", std::uint64_t{1} << 32}}, 79);
    CHECK(invalid_ppq.contains("error"));
}

TEST_CASE("Score MCP authors articulation mappings and exposes compiled control evidence",
          "[mcp][score][articulation-mapping]") {
    McpServer server;
    McpSession session;
    register_score_tools(server, nullptr, session.score);

    const auto created =
        call_tool(server,
                  "score_create",
                  {{"total_bars", 1}, {"parts", {{{"name", "Violin"}, {"instrument_type", 1}}}}},
                  90);
    REQUIRE(created.contains("score_id"));
    const auto score_id = created["score_id"];
    const auto part_id = created["part_ids"][0];
    const auto inserted = call_tool(server,
                                    "score_insert_note",
                                    {{"score_id", score_id},
                                     {"part_id", part_id},
                                     {"bar", 1},
                                     {"pitch", {{"letter", "C"}, {"accidental", 0}, {"octave", 4}}},
                                     {"duration", {{"n", 1}, {"d", 4}}},
                                     {"velocity", 80},
                                     {"articulation", 0}},
                                    91);
    REQUIRE(inserted["ok"] == true);

    const json mapping = {
        {"type", 5},
        {"combined",
         {{{"type", 0}, {"keyswitch_pitch", {{"letter", 0}, {"accidental", 0}, {"octave", 1}}}},
          {{"type", 1}, {"cc_number", 64}, {"cc_value", 127}},
          {{"type", 4}, {"program", 41}}}}};
    const auto mapped = call_tool(
        server,
        "score_set_articulation_mapping",
        {{"score_id", score_id}, {"part_id", part_id}, {"articulation", 0}, {"mapping", mapping}},
        92);
    REQUIRE(mapped["ok"] == true);

    const auto compiled = call_tool(server, "score_compile_to_midi", {{"score_id", score_id}}, 93);
    REQUIRE(compiled["notes"].size() == 1);
    REQUIRE(compiled["keyswitches"].size() == 1);
    CHECK(compiled["keyswitches"][0]["note"] == 24);
    REQUIRE(compiled["control_changes"].size() == 1);
    CHECK(compiled["control_changes"][0]["controller"] == 64);
    REQUIRE(compiled["program_changes"].size() == 1);
    CHECK(compiled["program_changes"][0]["program"] == 41);
    CHECK(compiled["report"]["articulation_mappings_applied"] == 1);
    CHECK(compiled["report"]["articulations_defaulted"] == 0);

    const auto removed = call_tool(
        server,
        "score_set_articulation_mapping",
        {{"score_id", score_id}, {"part_id", part_id}, {"articulation", 0}, {"mapping", nullptr}},
        94);
    REQUIRE(removed["ok"] == true);
    const auto defaulted = call_tool(server, "score_compile_to_midi", {{"score_id", score_id}}, 95);
    CHECK(defaulted["keyswitches"].empty());
    CHECK(defaulted["control_changes"].empty());
    CHECK(defaulted["program_changes"].empty());
    CHECK(defaulted["report"]["articulations_defaulted"] == 1);
}

TEST_CASE("Score MCP distinguishes structural and MIDI compilation policy",
          "[mcp][score][validation]") {
    McpServer server;
    McpSession session;
    register_score_tools(server, nullptr, session.score);
    const auto created =
        call_tool(server,
                  "score_create",
                  {{"total_bars", 1}, {"parts", {{{"name", "Piano"}, {"instrument_type", 0}}}}},
                  96);
    REQUIRE(created.contains("score_id"));

    auto validation = call_tool(server, "score_validate", {{"score_id", created["score_id"]}}, 97);
    CHECK(validation["compilable"] == true);
    CHECK(validation["structurally_compilable"] == true);
    CHECK(validation["midi_compilable"] == true);

    auto* score = session.score->find(created["score_id"].get<std::uint64_t>());
    REQUIRE(score != nullptr);
    score->parts[0].definition.rendering.midi_channel = 0;
    validation = call_tool(server, "score_validate", {{"score_id", created["score_id"]}}, 98);
    CHECK(validation["valid"] == false);
    CHECK(validation["compilable"] == true);
    CHECK(validation["structurally_compilable"] == true);
    CHECK(validation["midi_compilable"] == false);
}

TEST_CASE("Every MCP validation surface preserves the complete diagnostic contract",
          "[mcp][validation][contract]") {
    McpServer server;
    McpSession session;
    register_score_tools(server, nullptr, session.score);
    register_timbre_tools(server, nullptr, session.timbre);
    register_mix_tools(server, nullptr, session.mix);
    register_corpus_tools(server, session.corpus);

    const auto score =
        call_tool(server,
                  "score_create",
                  {{"total_bars", 1}, {"parts", {{{"name", "Piano"}, {"instrument_type", 0}}}}},
                  981);
    auto* stored_score = session.score->find(score["score_id"].get<std::uint64_t>());
    REQUIRE(stored_score != nullptr);
    stored_score->parts[0].definition.rendering.midi_channel = 0;

    const auto profile = call_tool(
        server, "create_timbre_profile", {{"part_id", 1}, {"name", "Invalid source"}}, 982);
    auto* stored_profile = session.timbre->find(profile["profile_id"].get<std::uint64_t>());
    REQUIRE(stored_profile != nullptr);
    stored_profile->source.data = sunny::core::SubtractiveSynth{};

    const auto graph = call_tool(server, "create_mix_graph", {{"part_ids", {1, 2}}}, 983);
    auto* stored_graph = session.mix->find(graph["graph_id"].get<std::uint64_t>());
    REQUIRE(stored_graph != nullptr);
    stored_graph->channels[1].id = stored_graph->channels[0].id;

    const auto work = call_tool(server,
                                "create_ingested_work",
                                {{"title", "Low confidence"}, {"source_format", "manual"}},
                                984);
    session.corpus->corpus.works.at(work["work_id"].get<std::uint64_t>())
        .ingestion_confidence.key_confidence = 0.1F;

    const std::array responses = {
        call_tool(server, "score_validate", {{"score_id", score["score_id"]}}, 985),
        call_tool(server, "validate_timbre", {{"profile_id", profile["profile_id"]}}, 986),
        call_tool(server, "validate_mix", {{"graph_id", graph["graph_id"]}}, 987),
        call_tool(server, "validate_corpus", json::object(), 988),
    };
    for (const auto& response : responses) {
        REQUIRE_FALSE(response["diagnostics"].empty());
        for (const auto& diagnostic : response["diagnostics"])
            check_diagnostic_contract(diagnostic);
    }

    REQUIRE(responses[0]["diagnostics"][0].contains("part_id"));
    REQUIRE(responses[1]["diagnostics"][0].contains("part_id"));
}

TEST_CASE("Timbre MCP rejects integer narrowing before mutation",
          "[mcp][validation][timbre][transaction]") {
    McpServer server;
    register_timbre_tools(server);

    auto invalid_profile =
        call_tool(server, "create_timbre_profile", {{"part_id", -1}, {"name", "Invalid"}}, 79);
    CHECK(invalid_profile.contains("error"));

    auto profile =
        call_tool(server, "create_timbre_profile", {{"part_id", 1}, {"name", "Valid"}}, 80);
    REQUIRE(profile["profile_id"] == 1);

    auto invalid_macro = call_tool(
        server, "create_macro", {{"profile_id", 1}, {"index", 256}, {"name", "Invalid"}}, 81);
    CHECK(invalid_macro.contains("error"));

    auto macro =
        call_tool(server, "create_macro", {{"profile_id", 1}, {"index", 0}, {"name", "Valid"}}, 82);
    CHECK(macro["success"] == true);

    auto invalid_set = call_tool(
        server, "set_macro", {{"profile_id", 1}, {"macro_index", 256}, {"value", 0.5}}, 83);
    CHECK(invalid_set.contains("error"));

    auto invalid_source =
        call_tool(server,
                  "set_sound_source",
                  {{"profile_id", 1}, {"source_type", "additive"}, {"partial_count", 65536}},
                  84);
    CHECK(invalid_source.contains("error"));
}

TEST_CASE("Corpus MCP failures preserve state and ID sequences",
          "[mcp][validation][corpus][transaction]") {
    McpServer server;
    register_corpus_tools(server);

    auto bad_year =
        call_tool(server, "create_composer_profile", {{"name", "Invalid"}, {"birth_year", -1}}, 80);
    CHECK(bad_year["error"] == "birth_year must be between 0 and 65535");

    auto composer = call_tool(server, "create_composer_profile", {{"name", "Valid composer"}}, 81);
    REQUIRE(composer["composer_id"] == 1);

    auto missing_composer =
        call_tool(server,
                  "create_ingested_work",
                  {{"title", "Orphan"}, {"source_format", "manual"}, {"composer_id", 999}},
                  82);
    CHECK(missing_composer["error"] == "composer not found");

    auto malformed_base64 =
        call_tool(server,
                  "ingest_midi",
                  {{"midi_base64", "TQ?="}, {"title", "Malformed"}, {"composer_id", 1}},
                  83);
    CHECK(malformed_base64["error"] == "invalid base64 data");

    auto invalid_grid = call_tool(server,
                                  "ingest_midi",
                                  {{"midi_base64", "TQ=="},
                                   {"title", "Invalid grid"},
                                   {"composer_id", 1},
                                   {"quantise_grid", 0}},
                                  84);
    CHECK(invalid_grid["error"] == "quantise_grid must be between 1 and 65535");

    auto invalid_xml =
        call_tool(server,
                  "ingest_musicxml",
                  {{"musicxml", "not xml"}, {"title", "Invalid XML"}, {"composer_id", 1}},
                  85);
    CHECK(invalid_xml["error"] == "MusicXML ingestion failed");

    auto batch =
        call_tool(server,
                  "ingest_batch",
                  {{"works", {{{"title", "Unknown"}, {"composer_id", 1}, {"format", "unknown"}}}}},
                  86);
    REQUIRE(batch["results"].size() == 1);
    CHECK(batch["results"][0]["error"] == "unknown format: unknown");

    auto created = call_tool(server,
                             "create_ingested_work",
                             {{"title", "Stable"}, {"source_format", "manual"}, {"composer_id", 1}},
                             87);
    REQUIRE(created["work_id"] == 1);

    auto rejected_update =
        call_tool(server,
                  "set_work_metadata",
                  {{"work_id", 1}, {"title", "Partial mutation"}, {"year_composed", 70000}},
                  88);
    CHECK(rejected_update["error"] == "year_composed must be between 0 and 65535");

    REQUIRE_FALSE(
        call_tool(
            server,
            "add_period_profile",
            {{"composer_id", 1}, {"label", "Early"}, {"year_start", 1900}, {"year_end", 1910}},
            881)
            .contains("error"));
    auto rejected_period =
        call_tool(server,
                  "set_work_metadata",
                  {{"work_id", 1}, {"title", "Partial period mutation"}, {"period", "Missing"}},
                  882);
    CHECK(rejected_period.contains("error"));

    auto corpus = call_tool(server, "get_corpus_json", json::object(), 89);
    REQUIRE(corpus["works"].size() == 1);
    CHECK(corpus["works"]["1"]["metadata"]["title"] == "Stable");
    CHECK(corpus["works"]["1"]["metadata"]["period"].is_null());
    CHECK(corpus["composers"]["1"]["works"].size() == 1);

    REQUIRE_FALSE(call_tool(server,
                            "set_work_metadata",
                            {{"work_id", 1}, {"title", "Updated"}, {"period", "Early"}},
                            891)
                      .contains("error"));
    corpus = call_tool(server, "get_corpus_json", json::object(), 892);
    CHECK(corpus["works"]["1"]["metadata"]["period"] == "Early");
    CHECK(corpus["composers"]["1"]["period_profiles"][0]["works"] == json::array({1}));

    REQUIRE_FALSE(call_tool(server, "set_work_metadata", {{"work_id", 1}, {"period", ""}}, 893)
                      .contains("error"));
    REQUIRE_FALSE(
        call_tool(server, "remove_ingested_work", {{"work_id", 1}}, 894).contains("error"));
    corpus = call_tool(server, "get_corpus_json", json::object(), 895);
    CHECK(corpus["works"].empty());
    CHECK(corpus["composers"]["1"]["works"].empty());
    CHECK(corpus["composers"]["1"]["period_profiles"][0]["works"].empty());

    auto after_removal =
        call_tool(server,
                  "create_ingested_work",
                  {{"title", "Monotonic"}, {"source_format", "manual"}, {"composer_id", 1}},
                  896);
    CHECK(after_removal["work_id"] == 2);
}

TEST_CASE("Mix MCP failures preserve ID sequences", "[mcp][validation][mix][transaction]") {
    McpServer server;
    register_mix_tools(server);

    auto invalid_graph = call_tool(server, "create_mix_graph", {{"part_ids", {-1}}}, 89);
    CHECK(invalid_graph.contains("error"));

    auto graph = call_tool(server, "create_mix_graph", {{"part_ids", {1}}}, 90);
    REQUIRE(graph["graph_id"] == 1);

    auto bad_group =
        call_tool(server,
                  "create_group_bus",
                  {{"graph_id", 1}, {"name", "Invalid"}, {"member_channel_ids", {999}}},
                  91);
    CHECK(bad_group["error"] == "Failed to create group bus");

    auto group = call_tool(server, "create_group_bus", {{"graph_id", 1}, {"name", "Valid"}}, 92);
    CHECK(group["group_bus_id"] == 1);
    auto parent = call_tool(server, "create_group_bus", {{"graph_id", 1}, {"name", "Parent"}}, 921);
    CHECK(parent["group_bus_id"] == 2);
    CHECK(call_tool(server,
                    "assign_group_to_group",
                    {{"graph_id", 1}, {"child_group_id", 1}, {"parent_group_id", 2}},
                    922)["success"] == true);
    CHECK(call_tool(server,
                    "assign_group_to_group",
                    {{"graph_id", 1}, {"child_group_id", 2}, {"parent_group_id", 1}},
                    923)
              .contains("error"));

    auto bad_effect = call_tool(server,
                                "add_channel_effect",
                                {{"graph_id", 1}, {"channel_id", 999}, {"effect_type", "eq"}},
                                93);
    CHECK(bad_effect["error"] == "Channel not found");

    auto effect = call_tool(server,
                            "add_channel_effect",
                            {{"graph_id", 1}, {"channel_id", 1}, {"effect_type", "eq"}},
                            94);
    CHECK(effect["effect_id"] == 1);

    auto stored = call_tool(server, "get_mix_json", {{"graph_id", 1}}, 95);
    REQUIRE(stored["mix_ir"]["group_buses"].size() == 2);
    CHECK(stored["mix_ir"]["group_buses"][0]["output"]["type"] == 1);
    CHECK(stored["mix_ir"]["group_buses"][0]["output"]["parent_group_id"] == 2);
    CHECK(stored["mix_ir"]["group_buses"][1]["member_groups"] == json::array({1}));
    CHECK(stored["mix_ir"]["channels"][0]["insert_chain"].size() == 1);

    CHECK(call_tool(
              server, "route_group_to_master", {{"graph_id", 1}, {"group_id", 1}}, 96)["success"] ==
          true);
    stored = call_tool(server, "get_mix_json", {{"graph_id", 1}}, 97);
    CHECK(stored["mix_ir"]["group_buses"][0]["output"]["type"] == 0);
    CHECK(stored["mix_ir"]["group_buses"][1]["member_groups"].empty());
}
