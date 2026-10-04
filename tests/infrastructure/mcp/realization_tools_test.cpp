/** Product-boundary fencing and restart oracles; native positive cases use the Python bridge. */
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <sunny/infrastructure/ableton/realization_store.hpp>
#include <sunny/infrastructure/mcp/project_tools.hpp>
#include <sunny/infrastructure/mcp/score_tools.hpp>
#include <sunny/infrastructure/mcp/workspace_state.hpp>

using namespace sunny::infrastructure;
using namespace sunny::core;
using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
struct Directory {
    fs::path path =
        fs::temp_directory_path() / ("sunny-realization-product-" + new_workspace_namespace());
    Directory() { REQUIRE(fs::create_directory(path)); }
    ~Directory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

class LostReplyTransport final : public LomTransport {
  public:
    const McpSession* session = nullptr;
    unsigned mutations = 0;
    std::vector<LomRequest> requests;
    bool is_connected() const override { return true; }
    Result<std::optional<AbletonTargetProfile>> target_profile() override {
        return std::optional{modeled_target_profile({12, 4, 0, "12.4.0"})};
    }
    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override {
        FAIL("The product must dispatch the tokened managed operation");
        return {};
    }
    LomResponse send(const LomRequest& request) override {
        requests.push_back(request);
        if (request.property_or_method == "sunny_managed_context")
            return {true,
                    json{{"schema_version", 1},
                         {"bridge_instance", "bridge_fixture"},
                         {"document_token", "document_fixture"}},
                    std::nullopt,
                    LomDeliveryState::ResponseReceived};
        if (request.property_or_method == "sunny_managed_create_clip") {
            ++mutations;
            REQUIRE(session);
            REQUIRE(session->realization->store);
            const auto payload = std::get<json>(request.args.at(0));
            const auto token = payload.at("operation_id").get<std::string>();
            const auto* fenced = session->realization->store->find(token);
            REQUIRE(fenced); // Externally observable before the first native send.
            REQUIRE(fs::is_regular_file(session->realization->store->directory() / "ledger.json"));
            CHECK(fenced->dispatch_ordinal == 1);
            CHECK(fenced->intent.prepared.outcome == ManagedOperationOutcome::Prepared);
            CHECK(payload.at("clip_end") == 4.0);
            CHECK(payload.at("signature_numerator") == 4);
            CHECK(payload.at("signature_denominator") == 4);
            REQUIRE(payload.at("notes").size() == 1);
            CHECK(payload.at("notes").at(0) == json{{"pitch", 60},
                                                    {"start_time", 0.0},
                                                    {"duration", 1.0},
                                                    {"velocity", 80.0},
                                                    {"mute", false},
                                                    {"probability", 1.0},
                                                    {"velocity_deviation", 0.0},
                                                    {"release_velocity", 64.0}});
            return {false,
                    std::nullopt,
                    "Reply lost after native dispatch",
                    LomDeliveryState::SentWithoutValidResponse};
        }
        FAIL("Unexpected native request " << request.property_or_method);
        return {};
    }
};

struct Fixture {
    McpServer server;
    McpSession session;
    int request_id = 0;
    explicit Fixture(LomTransport* transport = nullptr) {
        register_score_tools(server, session.score);
        register_project_tools(server, session, transport);
        auto score = call("score_create",
                          {{"title", "Native phrase"},
                           {"total_bars", 1},
                           {"parts", {{{"name", "Piano"}, {"instrument_type", 47}}}}});
        REQUIRE(!score.contains("error"));
        auto note = call("score_insert_note",
                         {{"score_id", 1},
                          {"part_id", 1},
                          {"bar", 1},
                          {"pitch", {{"letter", "C"}, {"accidental", 0}, {"octave", 4}}},
                          {"duration", {{"n", 1}, {"d", 4}}},
                          {"velocity", 80}});
        REQUIRE(!note.contains("error"));
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
        return json::parse(response.at("result").at("content").at(0).at("text").get<std::string>());
    }
    json revision_arguments() const {
        return {{"score_id", 1},
                {"part_id", 1},
                {"expected_project_revision", session.project->projects.at(1).revision}};
    }
};
} // namespace

TEST_CASE("Native product requires durable namespace before any bridge operation",
          "[mcp][realization][fence]") {
    LostReplyTransport transport;
    Fixture fixture(&transport);
    transport.session = &fixture.session;
    const auto before = workspace_to_json(fixture.session);
    REQUIRE(before);
    const auto declined = fixture.call("project_realization_create", fixture.revision_arguments());
    CHECK(declined.at("success") == false);
    CHECK(declined.at("error").get<std::string>().find("Save this workspace durably") !=
          std::string::npos);
    CHECK(transport.requests.empty());
    const auto after = workspace_to_json(fixture.session);
    REQUIRE(after);
    CHECK(*after == *before);
}

TEST_CASE("Native creation is durably fenced and never duplicated after a lost reply",
          "[mcp][realization][fence][restart]") {
#ifdef _WIN32
    SKIP("Native mutation durability is qualified on the primary Linux Docker storage");
#else
    Directory directory;
    LostReplyTransport transport;
    std::string attempt_id;
    std::string namespace_id;
    const auto workspace = directory.path / "workspace.json";
    {
        Fixture fixture(&transport);
        transport.session = &fixture.session;
        const auto saved = fixture.call("workspace_save", {{"path", workspace.string()}});
        REQUIRE(saved.at("success") == true);
        REQUIRE(saved.at("durability_confirmed") == true);
        REQUIRE(saved.at("native_history_available") == true);
        namespace_id = fixture.session.realization->metadata.workspace_namespace;
        const auto outcome =
            fixture.call("project_realization_create", fixture.revision_arguments());
        INFO(outcome.dump());
        CHECK(outcome.at("success") == false);
        CHECK(outcome.at("dispatch_fenced") == true);
        CHECK(outcome.at("history_saved") == true);
        CHECK(outcome.at("retry_authorized") == false);
        attempt_id = outcome.at("attempt_id");
        CHECK(transport.mutations == 1);
        const auto repeated =
            fixture.call("project_realization_create", fixture.revision_arguments());
        CHECK(repeated.at("state") == "reconciliation_required");
        CHECK(repeated.at("attempt_id") == attempt_id);
        CHECK(transport.mutations == 1);
    }
    {
        Fixture reopened(&transport);
        transport.session = &reopened.session;
        REQUIRE(open_workspace(reopened.session, workspace));
        CHECK(reopened.session.realization->metadata.workspace_namespace == namespace_id);
        const auto repeated =
            reopened.call("project_realization_create", reopened.revision_arguments());
        CHECK(repeated.at("state") == "reconciliation_required");
        CHECK(repeated.at("attempt_id") == attempt_id);
        CHECK(transport.mutations == 1);
        const auto revised =
            reopened.call("project_realization_update", reopened.revision_arguments());
        CHECK(revised.at("state") == "reconciliation_required");
        CHECK(transport.mutations == 1);
    }
    {
        Fixture missing(&transport);
        transport.session = &missing.session;
        REQUIRE(open_workspace(missing.session, workspace));
        REQUIRE(fs::remove(directory.path / namespace_id / "ledger.json"));
        const auto declined =
            missing.call("project_realization_create", missing.revision_arguments());
        CHECK(declined.at("success") == false);
        CHECK(declined.at("error").get<std::string>().find("Native history is unavailable") !=
              std::string::npos);
        CHECK_FALSE(fs::exists(directory.path / namespace_id / "ledger.json"));
        CHECK(transport.mutations == 1);
    }
#endif
}

TEST_CASE("Workspace native namespace migration and malformed references are explicit",
          "[mcp][workspace][realization][migration]") {
    Fixture fixture;
    auto encoded = workspace_to_json(fixture.session);
    REQUIRE(encoded);
    CHECK(encoded->at("version") == 2);
    auto legacy = *encoded;
    legacy["version"] = 1;
    legacy.erase("native_realization");
    auto migrated = workspace_from_json(legacy);
    REQUIRE(migrated);
    CHECK(migrated->native_namespace_is_new);
    CHECK(migrated->native_realization.workspace_namespace.size() == 32);
    CHECK_FALSE(migrated->native_realization.history_base_directory);
    for (const auto& bad_id : {json(""), json(std::string(32, 'G')), json(12)}) {
        auto bad = *encoded;
        bad["native_realization"]["workspace_namespace"] = bad_id;
        CHECK_FALSE(workspace_from_json(bad));
    }
    auto forbidden_legacy = *encoded;
    forbidden_legacy["version"] = 1;
    CHECK_FALSE(workspace_from_json(forbidden_legacy));
}

TEST_CASE("Legacy backup recovery retains the migrated native namespace and dispatch fences",
          "[mcp][workspace][realization][migration][backup]") {
#ifdef _WIN32
    SKIP("Native dispatch uses primary POSIX Docker storage");
#else
    Directory directory;
    LostReplyTransport transport;
    const auto workspace = directory.path / "legacy.json";
    std::string namespace_id, attempt_id;
    {
        Fixture fixture(&transport);
        transport.session = &fixture.session;
        auto legacy = workspace_to_json(fixture.session);
        REQUIRE(legacy);
        (*legacy)["version"] = 1;
        legacy->erase("native_realization");
        {
            std::ofstream previous(workspace);
            previous << legacy->dump();
        }
        REQUIRE(fixture.call("workspace_save", {{"path", workspace.string()}}).at("success") ==
                true);
        namespace_id = fixture.session.realization->metadata.workspace_namespace;
        std::ifstream backup(workspace.string() + ".bak");
        const auto migrated = json::parse(backup);
        CHECK(migrated.at("version") == 2);
        CHECK(migrated.at("native_realization").at("workspace_namespace") == namespace_id);
        const auto result =
            fixture.call("project_realization_create", fixture.revision_arguments());
        INFO(result.dump());
        REQUIRE(result.contains("attempt_id"));
        attempt_id = result.at("attempt_id");
        CHECK(transport.mutations == 1);
        {
            std::ofstream corrupt(workspace);
            corrupt << "corrupt";
        }
    }
    Fixture recovered(&transport);
    transport.session = &recovered.session;
    REQUIRE(recover_workspace_backup(recovered.session, workspace, true));
    CHECK(recovered.session.realization->metadata.workspace_namespace == namespace_id);
    REQUIRE(recovered.call("workspace_save", {{"path", workspace.string()}}).at("success") == true);
    const auto repeated =
        recovered.call("project_realization_create", recovered.revision_arguments());
    INFO(repeated.dump());
    CHECK(repeated.at("state") == "reconciliation_required");
    CHECK(repeated.at("attempt_id") == attempt_id);
    CHECK(transport.mutations == 1);
#endif
}

namespace {
class QueryTransport final : public LomTransport {
  public:
    LostReplyTransport creation;
    std::vector<LomRequest> requests;
    LomResponse query_response{
        true,
        json{{"outcome", "unknown_epoch"}, {"document_token", "new_document_fixture"}},
        std::nullopt,
        LomDeliveryState::ResponseReceived};
    bool is_connected() const override { return true; }
    Result<std::optional<AbletonTargetProfile>> target_profile() override {
        return creation.target_profile();
    }
    LomResponse send_notes(const LomPath& path, const std::vector<LomNoteData>& notes) override {
        return creation.send_notes(path, notes);
    }
    LomResponse send(const LomRequest& request) override {
        requests.push_back(request);
        if (request.property_or_method == "sunny_managed_operation") {
            const auto query = std::get<json>(request.args.at(0));
            REQUIRE(query.size() == 2);
            CHECK(query.at("document_token") == "document_fixture");
            REQUIRE(creation.session);
            REQUIRE(creation.session->realization->store);
            REQUIRE(creation.session->realization->store->find(query.at("operation_id")));
            return query_response;
        }
        return creation.send(request);
    }
};

std::string ledger_bytes(const RealizationStore& store) {
    std::ifstream input(store.directory() / "ledger.json", std::ios::binary);
    REQUIRE(input);
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}
} // namespace

TEST_CASE("Original native token remains queryable without current workspace durability",
          "[mcp][realization][reconcile][readonly]") {
#ifdef _WIN32
    SKIP("Native fences use qualified POSIX durability");
#else
    Directory directory;
    QueryTransport transport;
    Fixture fixture(&transport);
    transport.creation.session = &fixture.session;
    REQUIRE(fixture.call("workspace_save", {{"path", (directory.path / "workspace.json").string()}})
                .at("success") == true);
    const auto uncertain = fixture.call("project_realization_create", fixture.revision_arguments());
    const std::string token = uncertain.at("attempt_id");
    auto store = fixture.session.realization->store;
    const auto original_intent = managed_receipt_to_json(store->find(token)->intent.prepared);
    const auto before_requests = transport.requests.size();
    fixture.session.realization->namespace_saved_durably = false;
    const auto reconciled = fixture.call("project_realization_reconcile", {{"attempt_id", token}});
    CHECK(reconciled.at("query_succeeded") == true);
    CHECK(reconciled.at("history_saved") == true);
    CHECK(reconciled.at("success") == true);
    CHECK(reconciled.at("actual_receipt").at("outcome") == "unknown_epoch");
    CHECK(reconciled.at("actual_receipt").at("journal").at("document_token") ==
          "new_document_fixture");
    CHECK(reconciled.at("mutation_retried") == false);
    REQUIRE(transport.requests.size() == before_requests + 1);
    CHECK(transport.requests.back().property_or_method == "sunny_managed_operation");
    CHECK(transport.creation.mutations == 1);
    CHECK(managed_receipt_to_json(store->find(token)->intent.prepared) == original_intent);
    CHECK_FALSE(fixture.session.realization->namespace_saved_durably);
    const auto after_query = transport.requests.size();
    const auto denied = fixture.call("project_realization_create", fixture.revision_arguments());
    CHECK(denied.at("success") == false);
    CHECK(denied.at("state") == "reconciliation_required");
    CHECK(denied.at("attempt_id") == token);
    CHECK(transport.requests.size() == after_query);
    CHECK(transport.creation.mutations == 1);
#endif
}

TEST_CASE("Actual token query evidence remains visible when native history cannot publish",
          "[mcp][realization][reconcile][readonly][durability]") {
#ifdef _WIN32
    SKIP("Native fences use qualified POSIX durability");
#else
    Directory directory;
    QueryTransport transport;
    Fixture fixture(&transport);
    transport.creation.session = &fixture.session;
    REQUIRE(fixture.call("workspace_save", {{"path", (directory.path / "workspace.json").string()}})
                .at("success") == true);
    const auto uncertain = fixture.call("project_realization_create", fixture.revision_arguments());
    const std::string token = uncertain.at("attempt_id");
    auto store = fixture.session.realization->store;
    const auto original_receipt = store->find(token)->evidence.back();
    const auto original_encoded = managed_receipt_to_json(original_receipt);
    const auto original_bytes = ledger_bytes(*store);
    auto different_evidence = original_receipt;
    different_evidence.error = "Independent injected evidence-write failure";
    const auto blocked =
        store->append_evidence(token, different_evidence, std::nullopt, [](auto phase) {
            return phase == RealizationStoreIoPhase::FileSync;
        });
    REQUIRE_FALSE(blocked);
    REQUIRE_FALSE(store->native_writes_available());
    REQUIRE(ledger_bytes(*store) == original_bytes);
    const auto before_requests = transport.requests.size();
    const auto reconciled = fixture.call("project_realization_reconcile", {{"attempt_id", token}});
    CHECK(reconciled.at("query_succeeded") == true);
    CHECK(reconciled.at("success") == false);
    CHECK(reconciled.at("history_saved") == false);
    CHECK(reconciled.at("actual_receipt").at("outcome") == "unknown_epoch");
    CHECK(reconciled.at("actual_receipt").at("journal").at("document_token") ==
          "new_document_fixture");
    CHECK(reconciled.at("mutation_retried") == false);
    CHECK(reconciled.contains("error"));
    REQUIRE(transport.requests.size() == before_requests + 1);
    CHECK(transport.requests.back().property_or_method == "sunny_managed_operation");
    CHECK(transport.creation.mutations == 1);
    CHECK(managed_receipt_to_json(store->find(token)->evidence.back()) == original_encoded);
    CHECK(store->find(token)->evidence.size() == 1);
    CHECK(ledger_bytes(*store) == original_bytes);
    CHECK_FALSE(store->native_writes_available());
    const auto after_query = transport.requests.size();
    const auto denied = fixture.call("project_realization_create", fixture.revision_arguments());
    CHECK(denied.at("success") == false);
    CHECK(denied.at("state") == "reconciliation_required");
    CHECK(transport.requests.size() == after_query);
    CHECK(transport.creation.mutations == 1);
#endif
}

TEST_CASE("Failed token query is not reported as freshly observed native evidence",
          "[mcp][realization][reconcile][readonly][truthful]") {
#ifdef _WIN32
    SKIP("Native fences use qualified POSIX durability");
#else
    Directory directory;
    QueryTransport transport;
    Fixture fixture(&transport);
    transport.creation.session = &fixture.session;
    REQUIRE(fixture.call("workspace_save", {{"path", (directory.path / "workspace.json").string()}})
                .at("success") == true);
    const auto uncertain = fixture.call("project_realization_create", fixture.revision_arguments());
    const std::string token = uncertain.at("attempt_id");
    const auto original =
        managed_receipt_to_json(fixture.session.realization->store->find(token)->evidence.back());
    transport.query_response = {
        false, std::nullopt, "Query disconnected before reply", LomDeliveryState::NotSent};
    const auto reconciled = fixture.call("project_realization_reconcile", {{"attempt_id", token}});
    CHECK(reconciled.at("query_succeeded") == false);
    CHECK(reconciled.at("success") == false);
    CHECK(reconciled.at("mutation_retried") == false);
    CHECK_FALSE(reconciled.contains("actual_receipt"));
    CHECK(reconciled.at("receipt").at("outcome") == original.at("outcome"));
    CHECK(reconciled.at("receipt").at("delivery") == original.at("delivery"));
    CHECK(reconciled.at("receipt").at("request") == original.at("request"));
    CHECK(reconciled.at("retry_authorized") == false);
    CHECK(transport.requests.back().property_or_method == "sunny_managed_operation");
    CHECK(transport.creation.mutations == 1);
#endif
}
