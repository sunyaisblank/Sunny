#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <sunny/infrastructure/ableton/authority_transport.hpp>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/mcp/project_tools.hpp>
using namespace sunny::infrastructure;
using namespace sunny::core;
using nlohmann::json;
namespace fs = std::filesystem;
namespace {
const std::string ns(32, '1'), bridge(32, 'b'), document(32, 'd'), scope_id(32, 'a');
struct Directory {
    fs::path path = fs::temp_directory_path() /
                    ("sunny-legacy-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Directory() { REQUIRE(fs::create_directory(path)); }
    ~Directory() {
        std::error_code e;
        fs::remove_all(path, e);
    }
};
std::shared_ptr<RealizationStore> open(Directory& d) {
    auto r = RealizationStore::open(d.path, ns, RealizationStoreMode::InitializeNew);
    REQUIRE(r);
    return std::move(*r);
}
class GatewayPeer final : public LomTransport {
  public:
    std::shared_ptr<RealizationStore> store;
    bool lose_ack = false, replace = false, partial = false, lose_query = false,
         replace_bridge = false;
    unsigned effects = 0, scopes = 0, reads = 0, context_reads = 0, family_frames = 0;
    std::map<std::string, json> journals;
    json scoped_value = 120.0;
    json effect_value = {{"property", "tempo"}, {"requested", 140.0}, {"observed", 140.0}};
    bool is_connected() const override { return true; }
    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override {
        FAIL("No raw notes escape");
        return {};
    }
    LomResponse send(const LomRequest& r) override {
        REQUIRE(LomProtocol::validate_request(r));
        const auto& name = r.property_or_method;
        if (name == "sunny_managed_context") {
            ++context_reads;
            return {true,
                    json{{"schema_version", 1},
                         {"bridge_instance", replace_bridge ? std::string(32, '9') : bridge},
                         {"document_token", replace ? std::string(32, 'c') : document}},
                    std::nullopt};
        }
        if (name == "tempo" && r.type == LomRequestType::GetProperty) {
            ++reads;
            return {true, 120.0, std::nullopt};
        }
        if (name == "sunny_managed_inspect_song_settings") {
            ++reads;
            return {true, json{{"native_mutation_started", false}}, std::nullopt};
        }
        if (name == "sunny_managed_preview_song_settings" || name == "sunny_managed_rebind" ||
            name == "sunny_ordinary_execute") {
            ++family_frames;
            return {false, std::nullopt, "Model reached original-family boundary"};
        }
        REQUIRE(r.type == LomRequestType::CallMethod);
        REQUIRE(r.args.size() == 1);
        const auto& p = std::get<json>(r.args.front());
        if (name == "sunny_legacy_scope" || name == "sunny_legacy_finish") {
            ++scopes;
            auto result = p;
            result["scope_id"] = p.at("scope_id").is_null() ? json(scope_id) : p.at("scope_id");
            result["outcome"] = name == "sunny_legacy_finish" ? "closed" : "ready";
            result["graph_revision"] = name == "sunny_legacy_finish" ? json{} : json(0);
            result["workflow_id"] = nullptr;
            result["error"] = nullptr;
            return {true, result, std::nullopt};
        }
        if (name == "sunny_legacy_read") {
            ++reads;
            auto result = p;
            result.erase("command");
            result["outcome"] = "observed";
            result["value"] = scoped_value;
            result["error"] = nullptr;
            return {true, result, std::nullopt};
        }
        const auto operation = p.at("operation_id").get<std::string>();
        if (name == "sunny_legacy_prepare") {
            REQUIRE(store);
            const auto& w = store->legacy_workflows().at(p.at("workflow_id").get<std::string>());
            REQUIRE(w.children.back().prepared.intent.dump() == p.dump());
            LegacyOperationReceipt original;
            original.intent = p;
            auto j = legacy_token(original);
            j["outcome"] = "prepared";
            j["native_mutation_started"] = false;
            j["started_calls"] = 0;
            j["returned_calls"] = 0;
            j["result"] = nullptr;
            j["diagnostic"] = nullptr;
            j["error"] = nullptr;
            journals[operation] = j;
            return {true, j, std::nullopt};
        }
        if (name == "sunny_legacy_operation" && lose_query)
            return {false, std::nullopt, "query not sent", LomDeliveryState::NotSent};
        if (name == "sunny_legacy_operation") return {true, journals.at(operation), std::nullopt};
        REQUIRE(name == "sunny_legacy_execute");
        ++effects;
        auto& j = journals.at(operation);
        j["outcome"] = partial ? "partial" : "acknowledged";
        j["native_mutation_started"] = true;
        j["started_calls"] = 1;
        j["returned_calls"] = partial ? 0 : 1;
        j["result"] = partial ? json{}
                              : json{{"value", effect_value},
                                     {"target_binding", std::string(32, 'f')},
                                     {"graph_revision", 0}};
        j["error"] = partial ? json("native setter failed after entry") : json{};
        if (lose_ack) {
            lose_ack = false;
            return {false, std::nullopt, "lost ACK", LomDeliveryState::SentWithoutValidResponse};
        }
        return {true, j, std::nullopt};
    }
};
std::shared_ptr<RequestControl> control(std::optional<NativeOrigin> origin = NativeOrigin{
                                            bridge, document}) {
    return std::make_shared<RequestControl>("1",
                                            "tools/call",
                                            std::chrono::steady_clock::now() + MCP_REQUEST_TIMEOUT,
                                            std::move(origin));
}
LomRequest song_preview(const std::string& method = "sunny_managed_preview_song_settings") {
    return LomProtocol::call_method(
        LomPaths::song(),
        method,
        {json::parse(
            R"({"document_token":"dddddddddddddddddddddddddddddddd","project_key":"project","binding_key":"part","expected_content_fingerprint":"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee","expected_note_identity_fingerprint":"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff","desired":{"tempo":140.0,"signature_numerator":4,"signature_denominator":4}})")});
}
LegacyWorkflowRecipe recipe(const LegacyPlanningAuthority& a) {
    LegacyWorkflowRecipe r;
    r.kind = "single_request";
    r.authority = a;
    r.commands = json::array(
        {{{"command", legacy_command(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0))},
          {"phase", 0}}});
    r.intent_fingerprint = *managed_detail::managed_digest(r.commands);
    return r;
}
} // namespace
TEST_CASE("Authority gateway fences before prepare and finalizes a legitimate musical setter",
          "[authority_transport]") {
    Directory d;
    GatewayPeer peer;
    peer.store = open(d);
    AuthorityLomTransport wrapper(peer, [&](bool) { return peer.store; }, [] { return ns; });
    RequestControlScope request(control());
    auto response = wrapper.send(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0));
    REQUIRE(response.success);
    REQUIRE(response.legacy_receipt);
    REQUIRE(peer.effects == 1);
    REQUIRE(peer.store->legacy_workflows().begin()->second.state == "completed");
    REQUIRE(peer.store->native_writes_available());
    auto duplicate = execute_legacy_operation(*response.legacy_receipt, peer);
    REQUIRE_FALSE(duplicate);
    REQUIRE(peer.effects == 1);
}
TEST_CASE("Scoped native observations preserve the original bridge value variants",
          "[authority_transport][value-codec]") {
    Directory d;
    GatewayPeer peer;
    peer.store = open(d);
    AuthorityLomTransport wrapper(peer, [&](bool) { return peer.store; }, [] { return ns; });
    RequestControlScope request(control());
    auto captured = wrapper.capture_legacy_authority();
    REQUIRE(captured);
    REQUIRE(captured->has_value());

    auto read = LomProtocol::get_property(LomPaths::song(), "tempo");
    SECTION("Floating scalar remains a double") {
        peer.scoped_value = 120.0;
    }
    SECTION("Integer scalar remains an int") {
        read = LomProtocol::get_property(LomPaths::song(), "signature_numerator");
        peer.scoped_value = 4;
    }
    SECTION("Boolean scalar remains a bool") {
        read = LomProtocol::get_property(LomPaths::song(), "is_playing");
        peer.scoped_value = false;
    }
    SECTION("Heterogeneous note dictionaries remain structured JSON") {
        read = LomProtocol::call_method(
            LomPaths::clip(0, 0),
            "get_all_notes_extended",
            {json::parse(
                R"({"return":["note_id","pitch","start_time","duration","velocity","mute","probability","velocity_deviation","release_velocity"]})")});
        peer.scoped_value = json::parse(
            R"({"notes":[{"note_id":7,"pitch":60,"start_time":0.0,"duration":1.0,"velocity":80.0,"mute":false,"probability":1.0,"velocity_deviation":0.0,"release_velocity":64.0}]})");
    }
    REQUIRE(LomProtocol::validate_request(read));
    const auto observed = wrapper.send(read);
    REQUIRE(observed.success);
    REQUIRE(observed.value);
    if (peer.scoped_value.is_number_float())
        REQUIRE(std::get<double>(*observed.value) == 120.0);
    else if (peer.scoped_value.is_number_integer())
        REQUIRE(std::get<int>(*observed.value) == 4);
    else if (peer.scoped_value.is_boolean())
        REQUIRE_FALSE(std::get<bool>(*observed.value));
    else
        REQUIRE(std::get<json>(*observed.value) == peer.scoped_value);
    REQUIRE(peer.reads == 1);
    REQUIRE(peer.effects == 0);
    REQUIRE(peer.store->legacy_workflows().empty());
}
TEST_CASE("Acknowledged native note IDs retain the integer vector expected by the compiler",
          "[authority_transport][value-codec]") {
    Directory d;
    GatewayPeer peer;
    peer.store = open(d);
    peer.effect_value = json::parse("[7,11]");
    AuthorityLomTransport wrapper(peer, [&](bool) { return peer.store; }, [] { return ns; });
    RequestControlScope request(control());
    const std::vector<LomNoteData> notes{{60, 0.0, 1.0, 80, false}, {67, 2.0, 0.5, 72, false}};
    const auto response = wrapper.send_notes(LomPaths::clip(0, 0), notes);
    REQUIRE(response.success);
    REQUIRE(response.value);
    const std::vector<int> expected{7, 11};
    REQUIRE(std::get<std::vector<int>>(*response.value) == expected);
    REQUIRE(response.legacy_receipt);
    REQUIRE(response.legacy_receipt->outcome == LegacyOperationOutcome::Acknowledged);
    REQUIRE(response.legacy_receipt->journal->at("result").at("value") == json::parse("[7,11]"));
    REQUIRE(peer.store->legacy_workflows().begin()->second.state == "completed");
    REQUIRE(peer.effects == 1);
}
TEST_CASE("Lost ACK blocks fresh IDs and restart is original-token query only",
          "[authority_transport]") {
    Directory d;
    GatewayPeer peer;
    peer.store = open(d);
    LegacyOperationReceipt original;
    {
        AuthorityLomTransport wrapper(peer, [&](bool) { return peer.store; }, [] { return ns; });
        RequestControlScope request(control());
        peer.lose_ack = true;
        auto response = wrapper.send(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0));
        REQUIRE_FALSE(response.success);
        REQUIRE(response.legacy_receipt);
        original = *response.legacy_receipt;
        REQUIRE(original.outcome == LegacyOperationOutcome::Indeterminate);
        REQUIRE(peer.effects == 1);
        REQUIRE(peer.store->has_unresolved_legacy_workflow());
        REQUIRE_FALSE(
            wrapper.send(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0)).success);
        REQUIRE(peer.effects == 1);
    }
    peer.store.reset();
    auto reopened = RealizationStore::open(d.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE(reopened);
    peer.store = std::move(*reopened);
    REQUIRE_FALSE(peer.store->native_writes_available());
    REQUIRE_FALSE(peer.store->fence_legacy_child(
        original.intent.at("workflow_id").get<std::string>(), original));
    AuthorityLomTransport restored(peer, [&](bool) { return peer.store; }, [] { return ns; });
    RequestControlScope query_request(control(std::nullopt));
    auto queried = reconcile_legacy_operation(original, restored);
    REQUIRE(queried->outcome == LegacyOperationOutcome::Acknowledged);
    REQUIRE(peer.effects == 1);
    REQUIRE(peer.store->append_legacy_evidence(original.intent.at("workflow_id").get<std::string>(),
                                               *queried));
    REQUIRE(peer.store->has_unresolved_legacy_workflow()); // ACK alone does not prove full project
                                                           // postconditions.
}
TEST_CASE("Partial native setter remains evidence and never compensates", "[authority_transport]") {
    Directory d;
    GatewayPeer peer;
    peer.store = open(d);
    peer.partial = true;
    AuthorityLomTransport wrapper(peer, [&](bool) { return peer.store; }, [] { return ns; });
    RequestControlScope request(control());
    auto response = wrapper.send(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0));
    REQUIRE_FALSE(response.success);
    REQUIRE(response.legacy_receipt->outcome == LegacyOperationOutcome::Partial);
    REQUIRE(peer.effects == 1);
    REQUIRE(peer.store->has_unresolved_legacy_workflow());
}
TEST_CASE("Read-only doctor requests need neither saved history nor graph capture",
          "[authority_transport]") {
    GatewayPeer peer;
    AuthorityLomTransport wrapper(
        peer,
        [](bool) -> std::shared_ptr<RealizationStore> { throw std::runtime_error("unsaved"); },
        [] { return ns; });
    auto c = control(std::nullopt);
    c->input_closed = true;
    RequestControlScope request(c);
    REQUIRE(wrapper.send(LomProtocol::get_property(LomPaths::song(), "tempo")).success);
    REQUIRE(peer.scopes == 0);
    REQUIRE(peer.effects == 0);
    REQUIRE_FALSE(
        wrapper.send(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0)).success);
    REQUIRE(peer.scopes == 0);
}
TEST_CASE("Admission absence and stale Bridge or Document never acquire worker-time authority",
          "[authority_transport][native-origin]") {
    Directory d;
    GatewayPeer peer;
    peer.store = open(d);
    AuthorityLomTransport wrapper(peer, [&](bool) { return peer.store; }, [] { return ns; });
    std::shared_ptr<RequestControl> admitted;
    SECTION("A context observation cannot fill absent admission origin") {
        admitted = control(std::nullopt);
    }
    SECTION("Replacement Bridge preserves D but invalidates the original pair") {
        admitted = control();
        peer.replace_bridge = true;
    }
    SECTION("Replacement Document is not adopted when the worker first observes it") {
        admitted = control();
        peer.replace = true;
    }
    RequestControlScope request(admitted);
    REQUIRE(
        wrapper.send(LomProtocol::call_method(LomPaths::song(), "sunny_managed_context")).success);
    // Even a valid read observed by this Control cannot become its preparation origin.
    REQUIRE_FALSE(wrapper.capture_legacy_authority());
    const auto declined = wrapper.send(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0));
    REQUIRE_FALSE(declined.success);
    REQUIRE(declined.delivery == LomDeliveryState::NotSent);
    REQUIRE(peer.scopes == 0);
    REQUIRE(peer.effects == 0);
    REQUIRE(peer.store->legacy_workflows().empty());
    if (!admitted->native_origin) {
        REQUIRE(declined.error->find("doctor_ableton") != std::string::npos);
        REQUIRE_FALSE(admitted->native_origin);
    }
}
TEST_CASE("Retained previews require original admission while pure inspection still drains",
          "[authority_transport][native-origin]") {
    GatewayPeer peer;
    AuthorityLomTransport wrapper(
        peer,
        [](bool) -> std::shared_ptr<RealizationStore> { throw std::runtime_error("unsaved"); },
        [] { return ns; });
    auto admitted = control();
    auto preview = song_preview();
    SECTION("Absent origin refuses before even the fresh context read") {
        admitted = control(std::nullopt);
    }
    SECTION("EOF refuses retaining a preview") {
        admitted->input_closed = true;
    }
    SECTION("Stale bridge refuses D-only preview after validating the full pair") {
        peer.replace_bridge = true;
    }
    SECTION("Different body D cannot borrow a matching admission origin") {
        std::get<json>(preview.args.front())["document_token"] = std::string(32, 'c');
    }
    SECTION("Matching pair reaches native preview without requiring a saved history") {}
    RequestControlScope request(admitted);
    const auto response = wrapper.send(preview);
    REQUIRE_FALSE(response.success); // The independent peer declines at its family boundary.
    const bool matching = admitted->native_origin && !admitted->input_closed &&
                          !peer.replace_bridge &&
                          std::get<json>(preview.args.front()).at("document_token") == document;
    REQUIRE(peer.family_frames == (matching ? 1 : 0));
    REQUIRE(response.delivery ==
            (matching ? LomDeliveryState::ResponseReceived : LomDeliveryState::NotSent));
    if (!admitted->native_origin || admitted->input_closed) REQUIRE(peer.context_reads == 0);
    REQUIRE(wrapper.send(song_preview("sunny_managed_inspect_song_settings")).success);
    REQUIRE(peer.scopes == 0);
    REQUIRE(peer.effects == 0);
}
TEST_CASE("Managed D-only and Ordinary B-D stateful frames use the original admission pair",
          "[authority_transport][native-origin]") {
    GatewayPeer peer;
    AuthorityLomTransport wrapper(
        peer,
        [](bool) -> std::shared_ptr<RealizationStore> { throw std::runtime_error("unused"); },
        [] { return ns; });
    auto frame = LomProtocol::call_method(
        LomPaths::song(),
        "sunny_managed_rebind",
        {json::parse(
            R"({"document_token":"dddddddddddddddddddddddddddddddd","project_key":"project","binding_key":"part","operation_id":"operation","expected_manifest":{"schema_version":1}})")});
    auto admitted = control();
    SECTION("Matching D-only Managed frame reaches the native boundary") {}
    SECTION("Absent admission refuses Managed") {
        admitted = control(std::nullopt);
    }
    SECTION("Changed Bridge refuses D-only Managed") {
        peer.replace_bridge = true;
    }
    SECTION("Changed Document refuses Managed") {
        peer.replace = true;
    }
    SECTION("Ordinary explicit Bridge cannot replace original admission") {
        frame = LomProtocol::call_method(
            LomPaths::song(),
            "sunny_ordinary_execute",
            {json::parse(
                R"({"schema_version":1,"bridge_instance":"cccccccccccccccccccccccccccccccc","document_token":"dddddddddddddddddddddddddddddddd","operation_id":"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee","fingerprint":"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"})")});
    }
    RequestControlScope request(admitted);
    REQUIRE(LomProtocol::validate_request(frame));
    const auto response = wrapper.send(frame);
    REQUIRE_FALSE(response.success);
    const bool matching = admitted->native_origin && !peer.replace_bridge && !peer.replace &&
                          frame.property_or_method == "sunny_managed_rebind";
    REQUIRE(peer.family_frames == (matching ? 1 : 0));
    REQUIRE(response.delivery ==
            (matching ? LomDeliveryState::ResponseReceived : LomDeliveryState::NotSent));
    REQUIRE(peer.scopes == 0);
    REQUIRE(peer.effects == 0);
}
TEST_CASE("Separate plan scope preserves epoch under fresh control and namespace changes",
          "[authority_transport]") {
    Directory d;
    GatewayPeer peer;
    peer.store = open(d);
    std::string current = ns;
    AuthorityLomTransport wrapper(peer, [&](bool) { return peer.store; }, [&] { return current; });
    LegacyPlanningAuthority a;
    {
        RequestControlScope request(control());
        auto captured = wrapper.capture_legacy_authority();
        REQUIRE(captured);
        a = **captured;
        REQUIRE(wrapper.send(LomProtocol::get_property(LomPaths::song(), "tempo")).success);
    }
    {
        RequestControlScope request(control());
        peer.replace = true;
        REQUIRE_FALSE(wrapper.activate_legacy_workflow(recipe(a)));
        REQUIRE(peer.store->legacy_workflows().empty());
    }
    peer.replace = false;
    current = std::string(32, '2');
    {
        RequestControlScope request(control());
        REQUIRE_FALSE(wrapper.activate_legacy_workflow(recipe(a)));
        REQUIRE(peer.effects == 0);
    }
}
TEST_CASE("Legacy store preserves old bytes until deliberate family publication",
          "[authority_transport]") {
    Directory d;
    auto store = open(d);
    const auto path = store->directory() / "ledger.json";
    std::ifstream f(path);
    std::string before((std::istreambuf_iterator<char>(f)), {});
    REQUIRE(json::parse(before).at("schema_version") == 2);
    store.reset();
    auto existing = RealizationStore::open(d.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE(existing);
    std::ifstream unchanged(path);
    std::string bytes((std::istreambuf_iterator<char>(unchanged)), {});
    REQUIRE(bytes == before);
    GatewayPeer peer;
    peer.store = std::move(*existing);
    AuthorityLomTransport wrapper(peer, [&](bool) { return peer.store; }, [] { return ns; });
    RequestControlScope request(control());
    REQUIRE(wrapper.send(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0)).success);
    std::ifstream upgraded(path);
    json value;
    upgraded >> value;
    REQUIRE(value.at("schema_version") == 3);
    REQUIRE(value.contains("ordinary_attempts"));
    REQUIRE(value.at("legacy_workflows").size() == 1);
}
TEST_CASE("Legacy operation dispatch permits are consumed across moves", "[authority_transport]") {
    Directory d;
    GatewayPeer peer;
    peer.store = open(d);
    AuthorityLomTransport wrapper(peer, [&](bool) { return peer.store; }, [] { return ns; });
    RequestControlScope request(control());
    auto a = wrapper.capture_legacy_authority();
    REQUIRE(a);
    auto r = recipe(**a);
    const std::string workflow(32, 'c');
    REQUIRE(peer.store->fence_legacy_workflow(workflow, r));
    auto prepared =
        prepare_legacy_operation(**a,
                                 workflow,
                                 std::string(32, 'e'),
                                 1,
                                 LomProtocol::set_property(LomPaths::song(), "tempo", 140.0));
    REQUIRE(prepared);
    auto fenced = peer.store->fence_legacy_child(workflow, *prepared);
    REQUIRE(fenced);
    LegacyDispatchPermit moved(std::move(*fenced));
    REQUIRE_FALSE(fenced->take_prepared());
    REQUIRE(moved.take_prepared());
    REQUIRE_FALSE(moved.take_prepared());
}
namespace {
json tool(McpServer& server, const std::string& name, const json& args, int id = 1) {
    auto response = server.process_request({{"jsonrpc", "2.0"},
                                            {"method", "tools/call"},
                                            {"params", {{"name", name}, {"arguments", args}}},
                                            {"id", id}});
    REQUIRE(response.contains("result"));
    return json::parse(response.at("result").at("content").at(0).at("text").get<std::string>());
}
} // namespace
TEST_CASE("MCP legacy request routes literal tempo through same durable gateway",
          "[authority_transport][mcp]") {
    Directory d;
    GatewayPeer peer;
    peer.store = open(d);
    McpSession session;
    session.realization->metadata.workspace_namespace = ns;
    session.realization->metadata.history_base_directory = d.path.string();
    session.realization->namespace_saved_durably = true;
    session.realization->store = peer.store;
    AuthorityLomTransport wrapper(
        peer,
        [&](bool) { return session.realization->store; },
        [&] { return session.realization->metadata.workspace_namespace; });
    McpServer server;
    register_project_tools(server, session, &wrapper);
    json params = json::parse(
        R"({"command":{"type":"set","path":"song","name":"tempo","args":[140.0]},"bridge_instance":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","document_token":"dddddddddddddddddddddddddddddddd"})");
    auto response = tool(server, "legacy_ableton_request", params);
    REQUIRE(response.at("success") == true);
    REQUIRE(response.at("workspace_namespace") == ns);
    REQUIRE(response.at("receipt").at("outcome") == "acknowledged");
    REQUIRE(response.at("value").at("observed") == 140.0);
    REQUIRE(peer.effects == 1);
    const auto& intent = response.at("receipt").at("intent");
    auto query = tool(server,
                      "legacy_ableton_reconcile",
                      {{"workspace_namespace", ns},
                       {"workflow_id", intent.at("workflow_id")},
                       {"operation_id", intent.at("operation_id")}},
                      2);
    REQUIRE(query.at("query_only") == true);
    REQUIRE(query.at("dispatch_permit") == false);
    REQUIRE(peer.effects == 1);
    auto history = tool(server,
                        "legacy_ableton_history",
                        {{"workspace_namespace", ns}, {"workflow_id", intent.at("workflow_id")}},
                        3);
    REQUIRE(history.at("children").size() == 1);
    REQUIRE(history.at("state") == "completed");
    params["document_token"] = std::string(32, 'c');
    auto stale = tool(server, "legacy_ableton_request", params, 4);
    REQUIRE(stale.at("success") == false);
    REQUIRE(stale.at("receipt").is_null());
    REQUIRE(peer.effects == 1);
    REQUIRE(peer.store->legacy_workflows().size() == 1);
}
TEST_CASE("MCP lost ACK remains typed and explicit disposition never resumes",
          "[authority_transport][mcp]") {
    Directory d;
    GatewayPeer peer;
    peer.store = open(d);
    peer.lose_ack = true;
    McpSession session;
    session.realization->metadata.workspace_namespace = ns;
    session.realization->metadata.history_base_directory = d.path.string();
    session.realization->namespace_saved_durably = true;
    session.realization->store = peer.store;
    AuthorityLomTransport wrapper(peer, [&](bool) { return peer.store; }, [] { return ns; });
    McpServer server;
    register_project_tools(server, session, &wrapper);
    json params{
        {"command",
         {{"type", "set"}, {"path", "song"}, {"name", "tempo"}, {"args", json::array({140.0})}}},
        {"bridge_instance", bridge},
        {"document_token", document}};
    auto response = tool(server, "legacy_ableton_request", params);
    REQUIRE(response.at("success") == false);
    REQUIRE(response.at("receipt").at("outcome") == "indeterminate");
    REQUIRE(peer.effects == 1);
    const auto id = response.at("receipt").at("intent").at("workflow_id");
    auto declined = tool(server, "legacy_ableton_request", params, 2);
    REQUIRE_FALSE(declined.at("success").get<bool>());
    REQUIRE(peer.effects == 1);
    auto disposed = tool(
        server,
        "legacy_ableton_dispose",
        {{"workspace_namespace", ns},
         {"workflow_id", id},
         {"retained_state",
          "Operator inspected and accepts the retained tempo change; no continuation requested"}},
        3);
    REQUIRE(disposed.at("success") == true);
    REQUIRE(disposed.at("native_mutation") == false);
    REQUIRE(peer.effects == 1);
    REQUIRE_FALSE(peer.store->has_unresolved_legacy_workflow());
}
TEST_CASE("MCP unsaved and reopened namespace history gates cannot fall through to native writes",
          "[authority_transport][mcp]") {
    Directory d;
    GatewayPeer peer;
    peer.store = open(d);
    bool saved = false;
    std::string current = ns;
    McpSession session;
    AuthorityLomTransport wrapper(
        peer,
        [&](bool mutation) {
            if (mutation && !saved) throw std::runtime_error("unsaved");
            return peer.store;
        },
        [&] { return current; });
    McpServer server;
    register_project_tools(server, session, &wrapper);
    json params{
        {"command",
         {{"type", "set"}, {"path", "song"}, {"name", "tempo"}, {"args", json::array({140.0})}}},
        {"bridge_instance", bridge},
        {"document_token", document}};
    auto unsaved = tool(server, "legacy_ableton_request", params);
    REQUIRE_FALSE(unsaved.at("success").get<bool>());
    REQUIRE(unsaved.at("receipt").is_null());
    REQUIRE(peer.store->legacy_workflows().empty());
    saved = true;
    current = std::string(32, '2');
    auto different = tool(server, "legacy_ableton_request", params, 2);
    REQUIRE_FALSE(different.at("success").get<bool>());
    REQUIRE(peer.effects == 0);
    REQUIRE(peer.store->legacy_workflows().empty());
}
TEST_CASE("Workflow fsync uncertainty grants no child dispatch capability",
          "[authority_transport]") {
    Directory d;
    auto store = open(d);
    LegacyPlanningAuthority a{json{{"schema_version", 1},
                                   {"bridge_instance", bridge},
                                   {"document_token", document},
                                   {"scope_id", scope_id}},
                              0,
                              ns};
    auto fenced = store->fence_legacy_workflow(
        std::string(32, 'c'), recipe(a), [](RealizationStoreIoPhase phase) {
            return phase == RealizationStoreIoPhase::DirectorySync;
        });
    REQUIRE_FALSE(fenced);
    REQUIRE(fenced.error().committed);
    REQUIRE_FALSE(store->native_writes_available());
    auto original =
        prepare_legacy_operation(a,
                                 std::string(32, 'c'),
                                 std::string(32, 'e'),
                                 1,
                                 LomProtocol::set_property(LomPaths::song(), "tempo", 140.0));
    REQUIRE(original);
    REQUIRE_FALSE(store->fence_legacy_child(std::string(32, 'c'), *original));
    store.reset();
    auto reopened = RealizationStore::open(d.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE(reopened);
    REQUIRE((*reopened)->has_unresolved_legacy_workflow());
    REQUIRE_FALSE((*reopened)->fence_legacy_child(std::string(32, 'c'), *original));
}
TEST_CASE("Expired request caches retain no namespace writer lock", "[authority_transport]") {
    Directory d;
    GatewayPeer peer;
    peer.store = open(d);
    AuthorityLomTransport wrapper(peer, [&](bool) { return peer.store; }, [] { return ns; });
    {
        RequestControlScope request(control());
        peer.lose_ack = true;
        REQUIRE_FALSE(
            wrapper.send(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0)).success);
    }
    peer.store.reset();
    auto reopened = RealizationStore::open(d.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE(reopened);
    peer.store = std::move(*reopened);
    REQUIRE(peer.store->has_unresolved_legacy_workflow());
    REQUIRE(peer.effects == 1);
}
TEST_CASE("Cancellation during scoped planning prevents preparation and records no effect",
          "[authority_transport]") {
    Directory d;
    GatewayPeer peer;
    peer.store = open(d);
    AuthorityLomTransport wrapper(peer, [&](bool) { return peer.store; }, [] { return ns; });
    auto c = control();
    RequestControlScope request(c);
    auto a = wrapper.capture_legacy_authority();
    REQUIRE(a);
    REQUIRE(wrapper.activate_legacy_workflow(recipe(**a)));
    c->cancelled = true;
    REQUIRE_FALSE(
        wrapper.send(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0)).success);
    REQUIRE(peer.effects == 0);
    REQUIRE(wrapper.finish_legacy_workflow(false));
    REQUIRE(peer.store->legacy_workflows().begin()->second.state == "not_applied");
}
TEST_CASE("An unsent recovery query cannot prove the original mutation unsent",
          "[authority_transport]") {
    Directory d;
    GatewayPeer peer;
    peer.store = open(d);
    peer.lose_ack = true;
    AuthorityLomTransport wrapper(peer, [&](bool) { return peer.store; }, [] { return ns; });
    RequestControlScope request(control());
    auto response = wrapper.send(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0));
    REQUIRE(response.legacy_receipt);
    auto original = *response.legacy_receipt;
    REQUIRE(original.stage == LegacyOperationStage::Execute);
    peer.lose_query = true;
    auto query = reconcile_legacy_operation(original, peer);
    REQUIRE(query);
    REQUIRE(query->outcome == LegacyOperationOutcome::NotSent);
    REQUIRE(query->stage == LegacyOperationStage::Query);
    REQUIRE(legacy_receipt_from_json(legacy_receipt_to_json(*query)));
    const auto workflow = original.intent.at("workflow_id").get<std::string>();
    REQUIRE(peer.store->append_legacy_evidence(workflow, *query));
    REQUIRE(peer.store->finalize_legacy_workflow(workflow, false));
    REQUIRE(peer.store->has_unresolved_legacy_workflow());
    REQUIRE(peer.effects == 1);
}
