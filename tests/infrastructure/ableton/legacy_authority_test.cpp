#include <catch2/catch_test_macros.hpp>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/legacy_authority.hpp>
#include <sunny/infrastructure/ableton/transport.hpp>
using namespace sunny::infrastructure;
using nlohmann::json;
namespace {
json golden_intent() {
    return json::parse(
        R"({"schema_version":1,"bridge_instance":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","document_token":"dddddddddddddddddddddddddddddddd","scope_id":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","workflow_id":"cccccccccccccccccccccccccccccccc","operation_id":"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee","ordinal":1,"graph_revision":0,"command":{"type":"set","path":"song","name":"tempo","args":[140.0]}})");
}
const char* fingerprint = "8e3e529fb67e726541d9c464e2a6a87208b4c7673b482805c21d15678e3e652c";
json golden_prepared() {
    return json::parse(
        R"({"schema_version":1,"bridge_instance":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","document_token":"dddddddddddddddddddddddddddddddd","scope_id":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","workflow_id":"cccccccccccccccccccccccccccccccc","operation_id":"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee","ordinal":1,"graph_revision":0,"fingerprint":"8e3e529fb67e726541d9c464e2a6a87208b4c7673b482805c21d15678e3e652c","outcome":"prepared","native_mutation_started":false,"started_calls":0,"returned_calls":0,"result":null,"diagnostic":null,"error":null})");
}
class LiteralPeer final : public LomTransport {
  public:
    json response = golden_prepared();
    unsigned sends = 0;
    bool is_connected() const override { return true; }
    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override {
        FAIL("No raw notes");
        return {};
    }
    LomResponse send(const LomRequest& r) override {
        REQUIRE(r.property_or_method == "sunny_legacy_operation");
        ++sends;
        return {true, response, std::nullopt};
    }
};
} // namespace
TEST_CASE("Legacy SM1 tempo vector is literal and preserves numeric type", "[legacy_authority]") {
    REQUIRE(*managed_detail::managed_digest(golden_intent()) == fingerprint);
    auto changed = golden_intent();
    changed["command"]["args"][0] = 140;
    REQUIRE(*managed_detail::managed_digest(changed) != fingerprint);
    REQUIRE(legacy_request_valid("sunny_legacy_prepare", golden_intent()));
    LegacyOperationReceipt r;
    r.intent = golden_intent();
    REQUIRE(legacy_token(r).at("fingerprint") == fingerprint);
    REQUIRE(legacy_receipt_from_json(legacy_receipt_to_json(r)));
}
TEST_CASE("Legacy algebra refuses recursion and noncanonical or mixed schemas",
          "[legacy_authority]") {
    auto v = golden_intent();
    v["extra"] = 0;
    REQUIRE_FALSE(legacy_request_valid("sunny_legacy_prepare", v));
    v = golden_intent();
    v.erase("ordinal");
    REQUIRE_FALSE(legacy_request_valid("sunny_legacy_prepare", v));
    v = golden_intent();
    v["ordinal"] = true;
    REQUIRE_FALSE(legacy_request_valid("sunny_legacy_prepare", v));
    v = golden_intent();
    v["ordinal"] = 1.0;
    REQUIRE_FALSE(legacy_request_valid("sunny_legacy_prepare", v));
    v = golden_intent();
    v["command"]["path"] = "song//";
    REQUIRE_FALSE(legacy_request_valid("sunny_legacy_prepare", v));
    v = golden_intent();
    v["command"]["type"] = "call";
    v["command"]["name"] = "sunny_legacy_execute";
    REQUIRE_FALSE(legacy_request_valid("sunny_legacy_prepare", v));
    REQUIRE_FALSE(LomProtocol::is_read_only_request(
        LomProtocol::call_method(LomPaths::song(), "sunny_managed_context", {false})));
    REQUIRE(
        LomProtocol::is_read_only_request(LomProtocol::get_property(LomPaths::song(), "tempo")));
    REQUIRE_FALSE(LomProtocol::is_read_only_request(
        LomProtocol::set_property(LomPaths::song(), "tempo", 140.0)));
}
TEST_CASE("Native frame codec extracts only validated top-level family epochs",
          "[legacy_authority][native-origin]") {
    const auto legacy =
        LomProtocol::call_method(LomPaths::song(), "sunny_legacy_prepare", {golden_intent()});
    REQUIRE(LomProtocol::requires_native_origin(legacy));
    auto origin = LomProtocol::native_frame_origin(legacy);
    REQUIRE(origin);
    REQUIRE(origin->document_token == "dddddddddddddddddddddddddddddddd");
    REQUIRE(origin->bridge_instance == "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

    const auto ordinary = LomProtocol::call_method(
        LomPaths::song(),
        "sunny_ordinary_execute",
        {json::parse(
            R"({"schema_version":1,"bridge_instance":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","document_token":"dddddddddddddddddddddddddddddddd","operation_id":"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee","fingerprint":"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"})")});
    REQUIRE(LomProtocol::requires_native_origin(ordinary));
    origin = LomProtocol::native_frame_origin(ordinary);
    REQUIRE(origin);
    REQUIRE(origin->document_token == "dddddddddddddddddddddddddddddddd");
    REQUIRE(origin->bridge_instance == "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

    const auto managed = LomProtocol::call_method(
        LomPaths::song(),
        "sunny_managed_rebind",
        {json::parse(
            R"({"document_token":"dddddddddddddddddddddddddddddddd","project_key":"project","binding_key":"part","operation_id":"operation","expected_manifest":{"schema_version":1}})")});
    REQUIRE(LomProtocol::requires_native_origin(managed));
    origin = LomProtocol::native_frame_origin(managed);
    REQUIRE(origin);
    REQUIRE(origin->document_token == "dddddddddddddddddddddddddddddddd");
    REQUIRE_FALSE(origin->bridge_instance);

    auto malformed = managed;
    std::get<json>(malformed.args.front())["document_token"] = "document-A";
    REQUIRE(
        LomProtocol::validate_request(malformed)); // The older Managed key grammar stays intact.
    REQUIRE_FALSE(LomProtocol::native_frame_origin(malformed)); // Admission IDs are strict hex.
    malformed = managed;
    std::get<json>(malformed.args.front())["bridge_instance"] = std::string(32, 'b');
    REQUIRE_FALSE(LomProtocol::validate_request(malformed));
    REQUIRE_FALSE(LomProtocol::native_frame_origin(malformed));
    malformed = legacy;
    std::get<json>(malformed.args.front())["document_token"] = true;
    REQUIRE_FALSE(LomProtocol::requires_native_origin(malformed));
    REQUIRE_FALSE(LomProtocol::native_frame_origin(malformed));
}
TEST_CASE("Read-only retained preview requires origin but doctor and history grant none",
          "[legacy_authority][native-origin]") {
    const auto preview = LomProtocol::call_method(
        LomPaths::song(),
        "sunny_managed_preview_song_settings",
        {json::parse(
            R"({"document_token":"dddddddddddddddddddddddddddddddd","project_key":"project","binding_key":"part","expected_content_fingerprint":"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee","expected_note_identity_fingerprint":"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff","desired":{"tempo":140.0,"signature_numerator":4,"signature_denominator":4}})")});
    REQUIRE(LomProtocol::is_read_only_request(preview));
    REQUIRE(LomProtocol::requires_native_origin(preview));
    const auto origin = LomProtocol::native_frame_origin(preview);
    REQUIRE(origin);
    REQUIRE(origin->document_token == "dddddddddddddddddddddddddddddddd");
    REQUIRE_FALSE(origin->bridge_instance);

    auto inspection = preview;
    inspection.property_or_method = "sunny_managed_inspect_song_settings";
    const auto query = LomProtocol::call_method(
        LomPaths::song(),
        "sunny_managed_operation",
        {json::parse(
            R"({"document_token":"dddddddddddddddddddddddddddddddd","operation_id":"original-operation"})")});
    for (const auto& read :
         {inspection,
          query,
          LomProtocol::get_property(LomPaths::song(), "tempo"),
          LomProtocol::call_method(LomPaths::song(), "sunny_managed_context"),
          LomProtocol::call_method(LomPaths::song(), "sunny_get_target_profile"),
          LomProtocol::call_method(LomPaths::song(), "sunny_get_remote_log", {0})}) {
        REQUIRE(LomProtocol::is_read_only_request(read));
        REQUIRE_FALSE(LomProtocol::requires_native_origin(read));
        REQUIRE_FALSE(LomProtocol::native_frame_origin(read));
    }
    // Merely spelling a retained preview method cannot bypass its closed schema.
    auto malformed = preview;
    std::get<json>(malformed.args.front()).erase("desired");
    REQUIRE_FALSE(LomProtocol::requires_native_origin(malformed));
    REQUIRE_FALSE(LomProtocol::native_frame_origin(malformed));
}
TEST_CASE("Raw musical commands cannot provide physical frame authority",
          "[legacy_authority][native-origin]") {
    for (const auto& raw :
         {LomProtocol::set_property(LomPaths::song(), "tempo", 140.0),
          LomProtocol::call_method(LomPaths::song(), "create_scene", {-1}),
          LomProtocol::call_method(LomPaths::clip_slot(0, 0), "create_clip", {4.0})}) {
        REQUIRE(LomProtocol::validate_request(raw));
        REQUIRE(LomProtocol::requires_native_origin(raw));
        REQUIRE_FALSE(LomProtocol::native_frame_origin(raw));
    }
}
TEST_CASE("One closed classification preserves read, preview and physical authority boundaries",
          "[legacy_authority][native-origin][classification]") {
    const auto typed =
        LomProtocol::call_method(LomPaths::song(), "sunny_legacy_prepare", {golden_intent()});
    auto classified = LomProtocol::classify_request(typed);
    REQUIRE(classified);
    REQUIRE_FALSE(classified->read_only);
    REQUIRE(classified->requires_native_origin);
    REQUIRE(classified->native_origin);
    REQUIRE(classified->native_origin->document_token == "dddddddddddddddddddddddddddddddd");
    REQUIRE(classified->native_origin->bridge_instance == "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

    classified =
        LomProtocol::classify_request(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0));
    REQUIRE(classified);
    REQUIRE_FALSE(classified->read_only);
    REQUIRE(classified->requires_native_origin);
    REQUIRE_FALSE(classified->native_origin); // A raw writer cannot become a physical permit.

    classified = LomProtocol::classify_request(
        LomProtocol::call_method(LomPaths::song(), "sunny_managed_context"));
    REQUIRE(classified);
    REQUIRE(classified->read_only);
    REQUIRE_FALSE(classified->requires_native_origin);
    REQUIRE_FALSE(classified->native_origin);

    const auto preview = LomProtocol::call_method(
        LomPaths::song(),
        "sunny_managed_preview_song_settings",
        {json::parse(
            R"({"document_token":"dddddddddddddddddddddddddddddddd","project_key":"project","binding_key":"part","expected_content_fingerprint":"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee","expected_note_identity_fingerprint":"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff","desired":{"tempo":140.0,"signature_numerator":4,"signature_denominator":4}})")});
    classified = LomProtocol::classify_request(preview);
    REQUIRE(classified);
    REQUIRE(classified->read_only);
    REQUIRE(classified->requires_native_origin);
    REQUIRE(classified->native_origin);
    REQUIRE(classified->native_origin->document_token == "dddddddddddddddddddddddddddddddd");
    REQUIRE_FALSE(classified->native_origin->bridge_instance);

    auto old_key = preview;
    std::get<json>(old_key.args.front())["document_token"] = "document-A";
    classified = LomProtocol::classify_request(old_key);
    REQUIRE(classified); // Valid historical Managed key grammar remains a closed request.
    REQUIRE(classified->read_only);
    REQUIRE(classified->requires_native_origin);
    REQUIRE_FALSE(classified->native_origin); // It cannot carry a strict admission epoch.
    REQUIRE(LomProtocol::is_read_only_request(old_key));
    REQUIRE(LomProtocol::requires_native_origin(old_key));
    REQUIRE_FALSE(LomProtocol::native_frame_origin(old_key));

    auto extra = typed;
    std::get<json>(extra.args.front())["extra"] = false;
    auto malformed_preview = preview;
    std::get<json>(malformed_preview.args.front()).erase("desired");
    for (const auto& malformed :
         {extra,
          malformed_preview,
          LomProtocol::call_method(LomPaths::song(), "sunny_managed_context", {false}),
          LomProtocol::set_property(LomPaths::song(), "tempo", true),
          LomProtocol::get_property(LomPath{{"song", ""}}, "tempo")}) {
        REQUIRE_FALSE(LomProtocol::classify_request(malformed));
        REQUIRE_FALSE(LomProtocol::is_read_only_request(malformed));
        REQUIRE_FALSE(LomProtocol::requires_native_origin(malformed));
        REQUIRE_FALSE(LomProtocol::native_frame_origin(malformed));
    }
}
TEST_CASE("Legacy receipts reject malformed native authority without granting retry",
          "[legacy_authority]") {
    LiteralPeer peer;
    LegacyOperationReceipt r;
    r.intent = golden_intent();
    auto observed = reconcile_legacy_operation(r, peer);
    REQUIRE(observed);
    REQUIRE(observed->outcome == LegacyOperationOutcome::NativePrepared);
    REQUIRE(legacy_receipt_from_json(legacy_receipt_to_json(*observed)));
    for (const auto* key : {"fingerprint", "scope_id", "graph_revision", "diagnostic"}) {
        peer.response = golden_prepared();
        peer.response.erase(key);
        auto bad = reconcile_legacy_operation(r, peer);
        REQUIRE(bad);
        REQUIRE(bad->outcome == LegacyOperationOutcome::Indeterminate);
        REQUIRE(legacy_receipt_from_json(legacy_receipt_to_json(*bad)));
    }
    peer.response = golden_prepared();
    peer.response["outcome"] = "unknown_epoch";
    auto unknown = reconcile_legacy_operation(r, peer);
    REQUIRE(unknown);
    REQUIRE(unknown->outcome == LegacyOperationOutcome::UnknownEpoch);
    REQUIRE(unknown->journal);
    REQUIRE_FALSE(execute_legacy_operation(*unknown, peer));
    REQUIRE(peer.sends == 6);
}
TEST_CASE("Legacy acknowledged journals need truthful phase counts and bounded diagnostics",
          "[legacy_authority]") {
    LiteralPeer peer;
    LegacyOperationReceipt r;
    r.intent = golden_intent();
    peer.response = golden_prepared();
    peer.response["outcome"] = "acknowledged";
    peer.response["native_mutation_started"] = true;
    peer.response["started_calls"] = 1;
    peer.response["returned_calls"] = 1;
    peer.response["result"] = {
        {"value", {{"property", "tempo"}, {"requested", 140.0}, {"observed", 140.0}}},
        {"target_binding", std::string(32, 'f')},
        {"graph_revision", 0}};
    auto ack = reconcile_legacy_operation(r, peer);
    REQUIRE(ack);
    REQUIRE(ack->outcome == LegacyOperationOutcome::Acknowledged);
    peer.response["started_calls"] = 2;
    peer.response["returned_calls"] = 2;
    REQUIRE(reconcile_legacy_operation(r, peer)->outcome == LegacyOperationOutcome::Indeterminate);
    peer.response["started_calls"] = 1;
    peer.response["returned_calls"] = 1;
    peer.response["result"]["graph_revision"] = 1;
    REQUIRE(reconcile_legacy_operation(r, peer)->outcome == LegacyOperationOutcome::Indeterminate);
    peer.response["result"]["graph_revision"] = 0;
    peer.response["returned_calls"] = 0;
    auto bad = reconcile_legacy_operation(r, peer);
    REQUIRE(bad->outcome == LegacyOperationOutcome::Indeterminate);
    peer.response["outcome"] = "partial";
    peer.response["result"] = nullptr;
    peer.response["error"] = "setter failed after entry";
    auto partial = reconcile_legacy_operation(r, peer);
    REQUIRE(partial->outcome == LegacyOperationOutcome::Partial);
    REQUIRE_FALSE(execute_legacy_operation(*partial, peer));
    peer.response["diagnostic"] = {{"phase", std::string(129, 'x')},
                                   {"target_binding", nullptr},
                                   {"graph_revision", 0},
                                   {"created", json::array()}};
    REQUIRE(reconcile_legacy_operation(r, peer)->outcome == LegacyOperationOutcome::Indeterminate);
}
