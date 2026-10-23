/** Ordinary typed receipts, durable fencing and orchestrator reconciliation.
 * These modeled native journals do not qualify a real Live host. */
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/dispatcher.hpp>

using namespace sunny::infrastructure;
using namespace sunny::core;
using nlohmann::json;
namespace fs = std::filesystem;
namespace {
const std::string ns(32, 'a');
const ManagedBridgeContext context{std::string(32, 'b'), std::string(32, 'c')};
json projection() {
    return {{"track_index", 0},
            {"slot_index", 0},
            {"clip_end", 4.0},
            {"notes",
             json::array({{{"pitch", 60},
                           {"start_time", 0.0},
                           {"duration", 1.0},
                           {"velocity", 100},
                           {"mute", false},
                           {"probability", 1.0},
                           {"velocity_deviation", 0.0},
                           {"release_velocity", 64.0}}})}};
}
json journal(const json& intent, const std::string& outcome = "prepared") {
    return {{"schema_version", 1},
            {"bridge_instance", intent.at("bridge_instance")},
            {"document_token", intent.at("document_token")},
            {"operation_id", intent.at("operation_id")},
            {"fingerprint", *managed_detail::managed_digest(intent)},
            {"action", intent.at("action")},
            {"outcome", outcome},
            {"native_mutation_started", false},
            {"started_calls", 0},
            {"returned_calls", 0},
            {"result", nullptr},
            {"error", nullptr}};
}
struct Directory {
    fs::path path = fs::temp_directory_path() /
                    ("sunny-ordinary-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Directory() { REQUIRE(fs::create_directory(path)); }
    ~Directory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};
std::shared_ptr<RealizationStore> store(Directory& directory) {
    auto opened = RealizationStore::open(directory.path, ns, RealizationStoreMode::InitializeNew);
    REQUIRE(opened);
    return std::move(*opened);
}
class NativeJournals final : public LomTransport {
  public:
    std::shared_ptr<RealizationStore> history;
    std::map<std::string, json> intents, journals;
    std::vector<std::string> names;
    unsigned mutations = 0;
    int generation = 0;
    bool clip = false, lose_next_execute = false, lose_next_prepare = false,
         throw_next_execute = false;
    bool is_connected() const override { return true; }
    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override {
        FAIL("Ordinary transaction must not use unguarded notes delivery");
        return {};
    }
    LomResponse send(const LomRequest& request) override {
        REQUIRE(LomProtocol::validate_request(request));
        names.push_back(request.property_or_method);
        if (request.property_or_method == "sunny_managed_context")
            return {true,
                    json{{"schema_version", 1},
                         {"bridge_instance", context.bridge_instance},
                         {"document_token", context.document_token}},
                    std::nullopt};
        REQUIRE(request.args.size() == 1);
        const auto& payload = std::get<json>(request.args[0]);
        const auto id = payload.at("operation_id").get<std::string>();
        if (request.property_or_method == "sunny_ordinary_prepare") {
            REQUIRE(history);
            REQUIRE(history->find_ordinary(id)); // durable before even prepare
            REQUIRE(history->find_ordinary(id)->prepared.intent.dump() == payload.dump());
            intents[id] = payload;
            journals[id] = journal(payload);
            if (std::exchange(lose_next_prepare, false))
                return {false,
                        std::nullopt,
                        "lost prepare reply",
                        LomDeliveryState::SentWithoutValidResponse};
            return {true, journals[id], std::nullopt};
        }
        if (request.property_or_method == "sunny_ordinary_operation")
            return {true, journals.at(id), std::nullopt};
        REQUIRE(request.property_or_method == "sunny_ordinary_execute");
        REQUIRE(history->find_ordinary(id));
        if (std::exchange(throw_next_execute, false))
            throw std::runtime_error("transport exception after possible send");
        ++mutations;
        const auto& intent = intents.at(id);
        const auto action = intent.at("action").get<std::string>();
        clip = action != "undo";
        if (clip) ++generation;
        auto result = journal(intent, "acknowledged");
        result["native_mutation_started"] = true;
        const int calls =
            action == "undo"
                ? 1
                : (action == "create" && intent.at("payload").at("notes").empty() ? 1 : 2);
        result["started_calls"] = calls;
        result["returned_calls"] = calls;
        result["result"] = {{"binding_token", std::string(32, 'd')},
                            {"generation", generation},
                            {"state", clip ? "clip" : "empty"},
                            {"content_fingerprint", std::string(64, 'e')}};
        journals[id] = result;
        if (std::exchange(lose_next_execute, false))
            return {false,
                    std::nullopt,
                    "lost execute reply",
                    LomDeliveryState::SentWithoutValidResponse};
        return {true, result, std::nullopt};
    }
};
void configure(BridgeDispatcher& dispatcher, const std::shared_ptr<RealizationStore>& history) {
    dispatcher.set_ordinary_store_provider([history](bool) { return history; });
}
} // namespace

TEST_CASE("Ordinary Clip codec closes immutable authority and native result",
          "[bridge][ordinary]") {
    auto prepared = prepare_ordinary_clip(context, std::string(32, '1'), "create", projection());
    REQUIRE(prepared);
    REQUIRE(ordinary_receipt_from_json(ordinary_receipt_to_json(*prepared)));
    SECTION("Missing and malformed authority are refused") {
        for (const auto* key :
             {"bridge_instance", "document_token", "operation_id", "action", "payload"}) {
            auto altered = prepared->intent;
            altered.erase(key);
            CHECK_FALSE(ordinary_request_valid("sunny_ordinary_prepare", altered));
        }
        auto altered = prepared->intent;
        altered["payload"]["track_index"] = true;
        CHECK_FALSE(ordinary_request_valid("sunny_ordinary_prepare", altered));
        altered = prepared->intent;
        altered["payload"]["notes"][0]["start_time"] = 4.0;
        CHECK_FALSE(ordinary_request_valid("sunny_ordinary_prepare", altered));
    }
    SECTION("Wrong journal identity cannot authorize inverse history") {
        auto encoded = ordinary_receipt_to_json(*prepared);
        encoded["delivery"] = 2;
        encoded["outcome"] = "native_prepared";
        encoded["journal"] = journal(prepared->intent);
        REQUIRE(ordinary_receipt_from_json(encoded));
        encoded["journal"]["operation_id"] = std::string(32, '2');
        CHECK_FALSE(ordinary_receipt_from_json(encoded));
    }
}

TEST_CASE("Ordinary durable permits are consumed once and restored fences never resend",
          "[bridge][ordinary][store]") {
    Directory directory;
    auto history = store(directory);
    auto prepared = prepare_ordinary_clip(context, std::string(32, '1'), "create", projection());
    REQUIRE(prepared);
    auto permit = history->fence_ordinary(*prepared);
    REQUIRE(permit);
    REQUIRE(permit->take_prepared());
    CHECK_FALSE(permit->take_prepared());
    CHECK_FALSE(history->fence_ordinary(*prepared));
    history.reset();
    auto reopened = RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE(reopened);
    REQUIRE((*reopened)->find_ordinary(std::string(32, '1')));
    CHECK_FALSE((*reopened)->fence_ordinary(*prepared));
}

TEST_CASE("Schema1 managed history remains unchanged until deliberate schema2 publication",
          "[bridge][ordinary][store]") {
    Directory directory;
    auto history = store(directory);
    const auto path = history->directory() / "ledger.json";
    history.reset();
    json legacy{{"format", "sunny-realization-ledger"},
                {"schema_version", 1},
                {"workspace_namespace", ns},
                {"attempts", json::array()}};
    const auto original = legacy.dump(2) + "\n";
    {
        std::ofstream file(path);
        file << original;
    }
    auto reopened = RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE(reopened);
    {
        std::ifstream file(path);
        const std::string actual((std::istreambuf_iterator<char>(file)), {});
        CHECK(actual == original);
    }
    auto prepared = prepare_ordinary_clip(context, std::string(32, '1'), "create", projection());
    REQUIRE(prepared);
    REQUIRE((*reopened)->fence_ordinary(*prepared));
    {
        std::ifstream file(path);
        json upgraded;
        file >> upgraded;
        CHECK(upgraded.at("schema_version") == 2);
        CHECK(upgraded.at("attempts") == legacy.at("attempts"));
    }
}

TEST_CASE("Ordinary fence failures prevent dispatch at every durable boundary",
          "[bridge][ordinary][store]") {
    for (auto phase : {RealizationStoreIoPhase::CreateTemporary,
                       RealizationStoreIoPhase::AfterPartialWrite,
                       RealizationStoreIoPhase::FileSync,
                       RealizationStoreIoPhase::Replace,
                       RealizationStoreIoPhase::DirectorySync}) {
        Directory directory;
        auto history = store(directory);
        auto prepared =
            prepare_ordinary_clip(context, std::string(32, '1'), "create", projection());
        REQUIRE(prepared);
        auto permit =
            history->fence_ordinary(*prepared, [phase](auto current) { return current == phase; });
        CHECK_FALSE(permit);
        CHECK_FALSE(history->native_writes_available());
    }
}

TEST_CASE("Ordinary musical create undo redo use only tokened native transactions",
          "[bridge][ordinary][orchestrator]") {
    Directory directory;
    NativeJournals transport;
    transport.history = store(directory);
    BridgeDispatcher dispatcher(&transport);
    configure(dispatcher, transport.history);
    Orchestrator orchestrator;
    SECTION("Progression") {
        REQUIRE(orchestrator.create_progression_clip(dispatcher, 0, 0, "C", "major", {"I", "V"})
                    .success());
    }
    SECTION("Rhythm") {
        REQUIRE(orchestrator.apply_euclidean_rhythm(dispatcher, 0, 0, 3, 8, 60).success());
    }
    SECTION("Empty rhythm") {
        REQUIRE(orchestrator.apply_euclidean_rhythm(dispatcher, 0, 0, 0, 8, 60).success());
    }
    SECTION("Arpeggio") {
        REQUIRE(orchestrator.apply_arpeggio(dispatcher, 0, 0, "C", "major", {"I"}, "up").success());
    }
    CHECK(transport.mutations == 1);
    CHECK(transport.clip);
    REQUIRE(orchestrator.undo(dispatcher).success());
    CHECK_FALSE(transport.clip);
    REQUIRE(orchestrator.redo(dispatcher).success());
    CHECK(transport.clip);
    CHECK(transport.mutations == 3);
    CHECK(transport.history->ordinary_attempts().size() == 3);
    CHECK(std::ranges::find(transport.names, "delete_clip") == transport.names.end());
}

TEST_CASE("Lost ordinary replies retain original tokens and subsequent calls query only",
          "[bridge][ordinary][orchestrator]") {
    Directory directory;
    NativeJournals transport;
    transport.history = store(directory);
    BridgeDispatcher dispatcher(&transport);
    configure(dispatcher, transport.history);
    Orchestrator orchestrator;
    SECTION("Lost create ACK") {
        transport.lose_next_execute = true;
        const auto uncertain =
            orchestrator.create_progression_clip(dispatcher, 0, 0, "C", "major", {"I"});
        CHECK(uncertain.outcome == OperationOutcome::Indeterminate);
        CHECK(transport.mutations == 1);
        REQUIRE(orchestrator.undo(dispatcher).success());
        CHECK(transport.names.back() == "sunny_ordinary_operation");
        CHECK(transport.mutations == 1);
        REQUIRE(orchestrator.undo(dispatcher).success());
        CHECK(transport.mutations == 2);
    }
    SECTION("Lost undo ACK") {
        REQUIRE(orchestrator.apply_euclidean_rhythm(dispatcher, 0, 0, 1, 4, 60).success());
        transport.lose_next_execute = true;
        CHECK(orchestrator.undo(dispatcher).outcome == OperationOutcome::Indeterminate);
        CHECK_FALSE(transport.clip);
        REQUIRE(orchestrator.undo(dispatcher).success());
        CHECK(transport.names.back() == "sunny_ordinary_operation");
        CHECK(transport.mutations == 2);
        REQUIRE(orchestrator.redo(dispatcher).success());
        CHECK(transport.mutations == 3);
    }
    SECTION("Lost prepare ACK cannot later execute automatically") {
        transport.lose_next_prepare = true;
        CHECK(orchestrator.apply_euclidean_rhythm(dispatcher, 0, 0, 1, 4, 60).outcome ==
              OperationOutcome::Indeterminate);
        CHECK(transport.mutations == 0);
        CHECK(orchestrator.undo(dispatcher).outcome == OperationOutcome::Indeterminate);
        CHECK(transport.names.back() == "sunny_ordinary_operation");
        CHECK(transport.mutations == 0);
    }
    SECTION("Post-fence exception preserves query-only uncertainty") {
        transport.throw_next_execute = true;
        CHECK(orchestrator.apply_euclidean_rhythm(dispatcher, 0, 0, 1, 4, 60).outcome ==
              OperationOutcome::Indeterminate);
        CHECK(transport.history->ordinary_attempts().size() == 1);
        CHECK(orchestrator.undo(dispatcher).outcome == OperationOutcome::Indeterminate);
        CHECK(transport.names.back() == "sunny_ordinary_operation");
        CHECK(transport.mutations == 0);
    }
}

TEST_CASE("Native history refuses a changed durable workspace namespace",
          "[bridge][ordinary][orchestrator]") {
    Directory first, second;
    NativeJournals transport;
    transport.history = store(first);
    BridgeDispatcher dispatcher(&transport);
    configure(dispatcher, transport.history);
    Orchestrator orchestrator;
    REQUIRE(orchestrator.apply_euclidean_rhythm(dispatcher, 0, 0, 1, 4, 60).success());
    auto other = RealizationStore::open(
        second.path, std::string(32, 'f'), RealizationStoreMode::InitializeNew);
    REQUIRE(other);
    std::shared_ptr<RealizationStore> replacement = std::move(*other);
    configure(dispatcher, replacement);
    CHECK(orchestrator.undo(dispatcher).outcome == OperationOutcome::NotApplied);
    CHECK(transport.mutations == 1);
}
