#include "managed_clip_geometry_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/managed_realization.hpp>

using namespace sunny::infrastructure;
using nlohmann::json;
namespace {
const ManagedBridgeContext geometry_context{"bridge_a", "document_a"};
json fixture() {
    return json::parse(MANAGED_CLIP_GEOMETRY_FIXTURE);
}
ManagedBindingReceipt before_binding() {
    return {geometry_context, "project_a", "part_a", fixture().at("before")};
}
LomRequest fixture_request() {
    return LomProtocol::call_method(LomPaths::song(),
                                    "sunny_managed_update_clip_geometry",
                                    {fixture().at("journal").at("request")});
}
void rehash_actual(json& observation) {
    observation["content_fingerprint"] =
        *managed_detail::managed_digest(observation.at("manifest"));
    observation["note_identity_fingerprint"] =
        *managed_detail::managed_digest(observation.at("note_identity"));
}
class GeometryPeer final : public LomTransport {
  public:
    json response = fixture().at("journal");
    bool lose_reply = false;
    std::vector<LomRequest> sent;
    LomResponse send(const LomRequest& request) override {
        sent.push_back(request);
        if (lose_reply) {
            lose_reply = false;
            return {false,
                    std::nullopt,
                    "lost native reply",
                    LomDeliveryState::SentWithoutValidResponse};
        }
        return {true, LomValue{response}, std::nullopt, LomDeliveryState::ResponseReceived};
    }
    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override { return {}; }
    bool is_connected() const noexcept override { return true; }
};
ManagedOperationReceipt execute(const json& journal) {
    const auto prepared = prepare_managed_operation(geometry_context, fixture_request());
    REQUIRE(prepared);
    GeometryPeer peer;
    peer.response = journal;
    const auto result = execute_managed_operation(*prepared, peer);
    REQUIRE(result);
    return *result;
}
} // namespace

TEST_CASE("Managed Clip geometry literal native receipt retains identities",
          "[managed][geometry]") {
    const auto request = make_managed_clip_geometry_request(
        geometry_context, "geometry_a", before_binding(), 8.0, 3, 8);
    REQUIRE(request);
    CHECK(LomProtocol::serialize_request(*request) ==
          LomProtocol::serialize_request(fixture_request()));
    const auto value = fixture();
    CHECK(value.at("journal").at("request_fingerprint") ==
          "e8893d2a3346e8a6cb05d53360df3d46b0e748183ab19d9b7ca9f64567972868");
    CHECK(value.at("journal").at("result").at("content_fingerprint") ==
          "392a60197268f08f1c31e30b941e9f262eea3852d60da3557e4b1b2566c66bce");
    CHECK(value.at("journal").at("result").at("note_identity_fingerprint") ==
          "8a5fc73380703250cf459592bdff2163d626b69e86905c2578d0f3eff4a47857");
    const auto receipt = execute(value.at("journal"));
    REQUIRE(receipt.outcome == ManagedOperationOutcome::Acknowledged);
    REQUIRE(receipt.journal);
    CHECK(*receipt.journal == value.at("journal"));
    const auto binding = managed_binding_receipt(receipt);
    REQUIRE(binding);
    CHECK(binding->observation.at("note_identity") == value.at("before").at("note_identity"));
    CHECK(binding->observation.at("manifest").at("clip").at("loop_end") == 4.0);
    REQUIRE(managed_binding_from_json(managed_binding_to_json(*binding)));
    REQUIRE(managed_receipt_from_json(managed_receipt_to_json(receipt)));
}

TEST_CASE("Managed Clip geometry guards every actual note tail and native state",
          "[managed][geometry]") {
    for (const auto end : {0.0, 2.0, 2.25, std::numeric_limits<double>::infinity()}) {
        INFO(end);
        CHECK_FALSE(make_managed_clip_geometry_request(
            geometry_context, "geometry_a", before_binding(), end, 4, 4));
    }
    CHECK(make_managed_clip_geometry_request(
        geometry_context, "geometry_a", before_binding(), 2.5, 4, 4));
    for (const auto numerator : {0, 100})
        CHECK_FALSE(make_managed_clip_geometry_request(
            geometry_context, "geometry_a", before_binding(), 8.0, numerator, 4));
    for (const auto denominator : {0, 3, 32})
        CHECK_FALSE(make_managed_clip_geometry_request(
            geometry_context, "geometry_a", before_binding(), 8.0, 4, denominator));
    CHECK_FALSE(make_managed_clip_geometry_request(
        geometry_context, "geometry_a", before_binding(), 4.0, 4, 4));
    auto foreign_context = geometry_context;
    foreign_context.document_token = "other_document";
    CHECK_FALSE(make_managed_clip_geometry_request(
        foreign_context, "geometry_a", before_binding(), 8.0, 4, 4));
    for (const auto state : {"is_playing", "is_recording", "is_triggered", "looping"}) {
        auto binding = before_binding();
        binding.observation["manifest"]["clip"][state] = true;
        binding.observation["structural_boundary_complete"] = false;
        rehash_actual(binding.observation);
        CHECK_FALSE(
            make_managed_clip_geometry_request(geometry_context, "geometry_a", binding, 8.0, 4, 4));
    }
    auto overflow = before_binding();
    for (auto* notes : {&overflow.observation["manifest"]["notes"],
                        &overflow.observation["note_identity"]["notes"]}) {
        (*notes)[0]["start_time"] = 1e308;
        (*notes)[0]["duration"] = 1e308;
    }
    rehash_actual(overflow.observation);
    CHECK_FALSE(make_managed_clip_geometry_request(
        geometry_context, "geometry_a", overflow, 1.7e308, 4, 4));
    auto incomplete = before_binding();
    incomplete.observation["manifest"]["entire_clip_population_observed"] = false;
    incomplete.observation["note_identity"]["entire_clip_population_observed"] = false;
    incomplete.observation["structural_boundary_complete"] = false;
    rehash_actual(incomplete.observation);
    CHECK_FALSE(
        make_managed_clip_geometry_request(geometry_context, "geometry_a", incomplete, 8.0, 4, 4));
}

TEST_CASE("Managed Clip geometry rejects contradictory phase or hashed flag evidence",
          "[managed][geometry]") {
    const auto reject = [](json malformed) {
        CHECK(execute(malformed).outcome == ManagedOperationOutcome::Indeterminate);
    };
    auto value = fixture().at("journal");
    value["native_mutation_started"] = false;
    reject(value);
    value = fixture().at("journal");
    value["progress"]["started_properties"] = {
        "signature_numerator", "end_marker", "signature_denominator"};
    reject(value);
    value = fixture().at("journal");
    value["result"]["clip_geometry_update"]["returned_properties"] = {"end_marker"};
    reject(value);
    value = fixture().at("journal");
    value["result"]["clip_geometry_update"]["before_device_identity_fingerprint"] = 1;
    reject(value);
    value = fixture().at("journal");
    value["result"]["manifest"]["clip"]["end_marker"] = 7.0;
    rehash_actual(value["result"]);
    reject(value);
    value = fixture().at("journal");
    value["result"]["note_identity"]["notes"][0]["velocity"] = 42.0;
    value["result"]["manifest"]["notes"][0]["velocity"] = 42.0;
    rehash_actual(value["result"]);
    reject(value);
    value = fixture().at("journal");
    auto& identities = value["result"]["note_identity"]["notes"];
    const auto first_id = identities[0]["note_id"];
    identities[0]["note_id"] = identities[1]["note_id"];
    identities[1]["note_id"] = first_id;
    rehash_actual(value["result"]);
    reject(value);
    value = fixture().at("journal");
    value["result"]["manifest"]["clip"]["loop_end"] = 7.0;
    rehash_actual(value["result"]);
    reject(value);
    value = fixture().at("journal");
    value["result"]["clip_geometry_update"]["before_note_identity_fingerprint"] =
        std::string(64, '0');
    reject(value);
}

TEST_CASE("Managed Clip geometry truthful mismatches and alias observations remain evidence",
          "[managed][geometry]") {
    auto value = fixture().at("journal");
    value["result"]["manifest"]["clip"]["end_marker"] = 7.0;
    value["result"]["clip_geometry_update"]["observed_geometry_matches_request"] = false;
    rehash_actual(value["result"]);
    const auto mismatched = execute(value);
    REQUIRE(mismatched.outcome == ManagedOperationOutcome::Acknowledged);
    REQUIRE(mismatched.journal);
    CHECK(*mismatched.journal == value);
    value = fixture().at("journal");
    value["result"]["manifest"]["clip"]["loop_end"] = 8.0;
    value["result"]["clip_geometry_update"]["loop_end_relationship"] = "followed_end_marker";
    rehash_actual(value["result"]);
    CHECK(execute(value).outcome == ManagedOperationOutcome::Acknowledged);
    value["result"]["manifest"]["clip"]["loop_end"] = 7.0;
    value["result"]["clip_geometry_update"]["loop_end_relationship"] = "unexpected_change";
    value["result"]["clip_geometry_update"]["other_finite_properties_preserved"] = false;
    rehash_actual(value["result"]);
    CHECK(execute(value).outcome == ManagedOperationOutcome::Acknowledged);
}

TEST_CASE("Managed Clip geometry lost reply queries once without replay", "[managed][geometry]") {
    const auto prepared = prepare_managed_operation(geometry_context, fixture_request());
    REQUIRE(prepared);
    GeometryPeer peer;
    peer.lose_reply = true;
    const auto uncertain = execute_managed_operation(*prepared, peer);
    REQUIRE(uncertain);
    CHECK(uncertain->outcome == ManagedOperationOutcome::Indeterminate);
    CHECK_FALSE(execute_managed_operation(*uncertain, peer));
    REQUIRE(peer.sent.size() == 1);
    const auto reconciled = reconcile_managed_operation(*uncertain, peer);
    REQUIRE(reconciled);
    CHECK(reconciled->outcome == ManagedOperationOutcome::Acknowledged);
    REQUIRE(peer.sent.size() == 2);
    CHECK(peer.sent.back().property_or_method == "sunny_managed_operation");
}

TEST_CASE("Managed Clip geometry known full reply bounds precede dispatch", "[managed][geometry]") {
    auto large = before_binding();
    auto& values = large.observation["manifest"]["notes"];
    auto& ids = large.observation["note_identity"]["notes"];
    const auto note = values[0];
    auto native = ids[0];
    values = json::array();
    ids = json::array();
    for (int i = 1; i <= 26000; ++i) {
        values.push_back(note);
        native["note_id"] = i;
        ids.push_back(native);
    }
    rehash_actual(large.observation);
    const auto request =
        make_managed_clip_geometry_request(geometry_context, "geometry_a", large, 8.0, 4, 4);
    REQUIRE_FALSE(request);
    CHECK(request.error() == sunny::core::ErrorCode::ManagedReplyCapacityExceeded);
}
