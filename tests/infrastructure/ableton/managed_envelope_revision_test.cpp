#include "managed_envelope_revision_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <sunny/infrastructure/ableton/detail/managed_envelope_revision.hpp>

using namespace sunny::infrastructure;
using nlohmann::json;
namespace {
const ManagedBridgeContext envelope_context{"bridge_a", "document_a"};
json envelope_fixture() {
    return json::parse(MANAGED_ENVELOPE_REVISION_FIXTURE);
}
class EnvelopePeer final : public LomTransport {
  public:
    json response;
    std::vector<LomRequest> sent;
    LomResponse send(const LomRequest& request) override {
        sent.push_back(request);
        return {true, LomValue{response}, std::nullopt, LomDeliveryState::ResponseReceived};
    }
    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override { return {}; }
    bool is_connected() const noexcept override { return true; }
};
ManagedOperationReceipt envelope_execute(const json& journal) {
    const auto request = LomProtocol::call_method(LomPaths::song(),
                                                  managed_envelope_detail::replace_method,
                                                  {envelope_fixture().at("journal").at("request")});
    const auto prepared = prepare_managed_operation(envelope_context, request);
    REQUIRE(prepared);
    EnvelopePeer peer;
    peer.response = journal;
    const auto result = execute_managed_operation(*prepared, peer);
    REQUIRE(result);
    INFO(result->error.value_or("no error"));
    return *result;
}
void rehash_preview(json& preview) {
    auto body = preview;
    body.erase("preview_fingerprint");
    preview["preview_fingerprint"] = *managed_detail::managed_digest(body);
}
} // namespace
TEST_CASE("Managed envelope literal preview and write evidence close independently",
          "[managed][envelope-revision]") {
    const auto fixture = envelope_fixture();
    const auto preview = managed_envelope_replacement_preview_from_json(fixture.at("preview"));
    REQUIRE(preview);
    CHECK(preview->preview_fingerprint ==
          "4670c565be4a9382ebb2a0f602cbc5bcd28f601432a487ea84da60071a934e23");
    CHECK(fixture.at("journal").at("request_fingerprint") ==
          "6c4e60ac821e978e10880c8deb2264c9dc29d3c665b0e4c786f168db6be3e234");
    const auto request =
        make_managed_envelope_replacement_request(envelope_context, "replace_a", *preview);
    REQUIRE(request);
    CHECK(std::get<json>(request->args[0]) == fixture.at("journal").at("request"));
    const auto receipt = envelope_execute(fixture.at("journal"));
    REQUIRE(receipt.outcome == ManagedOperationOutcome::Acknowledged);
    REQUIRE(receipt.journal);
    CHECK(*receipt.journal == fixture.at("journal"));
    REQUIRE(managed_binding_receipt(receipt));
    REQUIRE(managed_receipt_from_json(managed_receipt_to_json(receipt)));
    EnvelopePeer peer;
    peer.response = fixture.at("preview");
    const ManagedBindingReceipt binding{
        envelope_context, "project_a", "part_a", fixture.at("preview").at("observation")};
    const auto fresh = preview_managed_envelope_replacement(
        envelope_context, binding, fixture.at("preview").at("lane"), peer);
    REQUIRE(fresh);
    CHECK(fresh->evidence == fixture.at("preview"));
    REQUIRE(peer.sent.size() == 1);
    CHECK(peer.sent[0].property_or_method == managed_envelope_detail::preview_method);
}
TEST_CASE("Managed envelope finite samples never establish breakpoint coverage",
          "[managed][envelope-revision]") {
    auto value = envelope_fixture().at("preview");
    value["scope"]["breakpoint_population_observed"] = true;
    rehash_preview(value);
    CHECK_FALSE(managed_envelope_replacement_preview_from_json(value));
    value = envelope_fixture().at("preview");
    value["selected_envelope"]["samples"][0]["value"] = 2.0;
    rehash_preview(value);
    CHECK_FALSE(managed_envelope_replacement_preview_from_json(value));
    value = envelope_fixture().at("preview");
    value["selected_envelope"]["samples"][0]["time"] = 0.0;
    rehash_preview(value);
    CHECK_FALSE(managed_envelope_replacement_preview_from_json(value));
    value = envelope_fixture().at("preview");
    value["observation"]["manifest"]["mixer"]["panning"]["min"] = -1;
    value["observation"]["content_fingerprint"] =
        *managed_detail::managed_digest(value["observation"]["manifest"]);
    rehash_preview(value);
    CHECK_FALSE(managed_envelope_replacement_preview_from_json(value));
    value = envelope_fixture().at("preview");
    value["lane"]["points"][1] = 1;
    CHECK_FALSE(managed_envelope_replacement_preview_from_json(value));
    auto lane = envelope_fixture().at("preview").at("lane");
    lane["points"] = json::array();
    for (int i = 0; i < 65; ++i)
        lane["points"].push_back(json{{"time", i / 64.0}, {"value", 0.0}});
    CHECK_FALSE(managed_envelope_detail::steps(lane));
    lane = envelope_fixture().at("preview").at("lane");
    lane["points"][1]["time"] = 3.9999999999999996;
    CHECK_FALSE(managed_envelope_detail::steps(lane));
}
TEST_CASE("Managed envelope receipt refuses echoed contradictions but retains truthful mismatch",
          "[managed][envelope-revision]") {
    auto value = envelope_fixture().at("journal");
    value["native_mutation_started"] = false;
    CHECK(envelope_execute(value).outcome == ManagedOperationOutcome::Indeterminate);
    value = envelope_fixture().at("journal");
    value["progress"]["returned_calls"] = {"clear_envelope"};
    CHECK(envelope_execute(value).outcome == ManagedOperationOutcome::Indeterminate);
    value = envelope_fixture().at("journal");
    value["result"]["envelope_replacement"]["actual_samples"][0]["value"] = 0.25;
    CHECK(envelope_execute(value).outcome == ManagedOperationOutcome::Indeterminate);
    value["result"]["envelope_replacement"]["observed_step_samples_match_request"] = false;
    const auto actual_mismatch = envelope_execute(value);
    CHECK(actual_mismatch.outcome == ManagedOperationOutcome::Acknowledged);
    REQUIRE(actual_mismatch.journal);
    CHECK(*actual_mismatch.journal == value);
    value = envelope_fixture().at("journal");
    value["result"]["envelope_replacement"]["actual_samples"] = json::array();
    value["result"]["envelope_replacement"]["observed_step_samples_match_request"] = false;
    CHECK(envelope_execute(value).outcome == ManagedOperationOutcome::Indeterminate);
    value = envelope_fixture().at("journal");
    value["result"]["envelope_replacement"]["preview_metadata"]["scope"]
         ["unsampled_state_preservation_proven"] = true;
    CHECK(envelope_execute(value).outcome == ManagedOperationOutcome::Indeterminate);
    value = envelope_fixture().at("journal");
    value["request"]["allow_unsampled_selected_state_overwrite"] = false;
    CHECK(envelope_execute(value).outcome == ManagedOperationOutcome::Indeterminate);
}
TEST_CASE("Managed envelope factory bounds complete known reply before fence",
          "[managed][envelope-revision]") {
    auto value = envelope_fixture().at("preview");
    auto& observation = value["observation"];
    const auto note = observation.at("manifest").at("notes")[0];
    auto native = observation.at("note_identity").at("notes")[0];
    observation["manifest"]["notes"] = json::array();
    observation["note_identity"]["notes"] = json::array();
    for (int id = 1; id <= 26000; ++id) {
        observation["manifest"]["notes"].push_back(note);
        native["note_id"] = id;
        observation["note_identity"]["notes"].push_back(native);
    }
    observation["content_fingerprint"] =
        *managed_detail::managed_digest(observation.at("manifest"));
    observation["note_identity_fingerprint"] =
        *managed_detail::managed_digest(observation.at("note_identity"));
    rehash_preview(value);
    const auto preview = managed_envelope_replacement_preview_from_json(value);
    REQUIRE(preview);
    const auto request =
        make_managed_envelope_replacement_request(envelope_context, "replace_a", *preview);
    REQUIRE_FALSE(request);
    CHECK(request.error() == sunny::core::ErrorCode::ManagedReplyCapacityExceeded);
}
