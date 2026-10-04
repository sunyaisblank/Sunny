#include "managed_recovery_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <sunny/infrastructure/ableton/detail/managed_recovery.hpp>
#include <sunny/infrastructure/ableton/managed_recovery.hpp>

using namespace sunny::infrastructure;
using nlohmann::json;
namespace {
const ManagedBridgeContext context{"bridge_a", "document_a"};
const json selector{{"track_index", 0}, {"slot_index", 0}};

void sign_observation(json& observation) {
    observation["content_fingerprint"] =
        *managed_detail::managed_digest(observation.at("manifest"));
    observation["note_identity_fingerprint"] =
        *managed_detail::managed_digest(observation.at("note_identity"));
}

void sign_preview(json& value) {
    sign_observation(value.at("observation"));
    value.erase("preview_fingerprint");
    value["preview_fingerprint"] = *managed_detail::managed_digest(value);
}

json intent() {
    return {
        {"document_token", "document_a"},
        {"project_key", "project_a"},
        {"binding_key", "part_a"},
        {"operation_id", "adopt_a"},
        {"preview_token", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
        {"preview_fingerprint", "075133e8c2da6c6a9d3cc983d000e479ab3b1e39c791e03db9127d9892fdecc3"},
        {"explicit_adoption", true}};
}

class Peer final : public LomTransport {
  public:
    std::vector<LomRequest> requests;
    json reply;
    LomResponse send(const LomRequest& request) override {
        requests.push_back(request);
        return {true, LomValue{reply}, std::nullopt, LomDeliveryState::ResponseReceived};
    }
    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override {
        return {false, std::nullopt, "Unexpected send_notes", LomDeliveryState::NotSent};
    }
    bool is_connected() const noexcept override { return true; }
};
} // namespace

TEST_CASE("Current native adoption preview has a literal typed fingerprint and "
          "no historical authority",
          "[infrastructure][ableton][managed-recovery]") {
    const auto value = recovery_test_fixture::preview();
    const auto parsed = managed_adoption_preview_from_json(value);
    REQUIRE(parsed);
    CHECK(parsed->preview_token == "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    CHECK(parsed->preview_fingerprint ==
          "075133e8c2da6c6a9d3cc983d000e479ab3b1e39c791e03db9127d9892fdecc3");
    CHECK(parsed->evidence.at("authority_origin") == "none");
    CHECK(parsed->evidence.at("historical_identity_proven") == false);
    CHECK(parsed->evidence.at("set_info").at("file_path").is_null());
    CHECK(parsed->evidence.at("observation").at("content_boundary_complete") == false);
    const auto& notes = parsed->evidence.at("observation").at("note_identity").at("notes");
    CHECK(notes.at(0).at("note_id") == 41);
    CHECK(notes.at(1).at("note_id") == 99);
    const auto request = make_managed_adoption_request(context, "adopt_a", *parsed);
    REQUIRE(request);
    CHECK(request->property_or_method == "sunny_managed_adopt_clip");
    CHECK(std::get<json>(request->args.at(0)) == intent());
    CHECK(managed_detail::adoption_acknowledgement_valid(intent(),
                                                         recovery_test_fixture::acknowledgement()));
}

TEST_CASE("Native recovery selection requests have closed independent boundaries",
          "[infrastructure][ableton][managed-recovery]") {
    const auto good =
        make_managed_adoption_preview_request(context, "project_a", "part_a", selector);
    REQUIRE(good);
    CHECK(good->property_or_method == "sunny_managed_preview_adoption");
    for (const auto& invalid : std::vector<json>{
             json::object(),
             {{"track_index", true}, {"slot_index", 0}},
             {{"track_index", -1}, {"slot_index", 0}},
             {{"track_index", std::uint64_t{2147483648}}, {"slot_index", 0}},
             {{"track_index", 0}, {"slot_index", 0}, {"authority", true}},
             {{"track_tag", ""}, {"clip_tag", "a"}},
             {{"track_tag", std::string(1025, 'a')}, {"clip_tag", "a"}},
             {{"track_tag", std::string(1, static_cast<char>(0xff))}, {"clip_tag", "a"}}}) {
        CHECK_FALSE(make_managed_adoption_preview_request(context, "project_a", "part_a", invalid));
    }
    CHECK(make_managed_adoption_preview_request(
        context, "project_a", "part_a", {{"track_index", INT32_MAX}, {"slot_index", INT32_MAX}}));
    CHECK(make_managed_adoption_preview_request(
        context, "project_a", "part_a", {{"track_tag", "a"}, {"clip_tag", "b"}}));
    CHECK_FALSE(
        make_managed_adoption_preview_request({"bridge_a", ""}, "project_a", "part_a", selector));
    CHECK_FALSE(make_managed_adoption_preview_request(context, "project|a", "part_a", selector));
    auto approval = intent();
    approval["explicit_adoption"] = 1;
    CHECK_FALSE(managed_detail::adoption_request_valid(approval));
    approval = intent();
    approval["preview_token"] = std::string(32, 'A');
    CHECK_FALSE(managed_detail::adoption_request_valid(approval));
    approval = intent();
    approval["retry_historical"] = true;
    CHECK_FALSE(managed_detail::adoption_request_valid(approval));
}

TEST_CASE("Self-consistent forged native previews cannot expand the finite "
          "adoption boundary",
          "[infrastructure][ableton][managed-recovery]") {
    const std::vector<std::function<void(json&)>> invalid{
        [](json& v) { v["authority_origin"] = "historical_receipt"; },
        [](json& v) { v["historical_identity_proven"] = true; },
        [](json& v) { v["allowed_domains"].push_back("device_parameters"); },
        [](json& v) { v["preserved_unknown_domains"].erase(0); },
        [](json& v) { v["unknown"] = true; },
        [](json& v) { v["selector"]["track_index"] = 1; },
        [](json& v) { v["set_info"]["persistent_uuid"] = "fake"; },
        [](json& v) { v["observation"]["observed_notes_match_request"] = true; },
        [](json& v) { v["observation"]["manifest"]["track"]["name"] = "User piano"; },
        [](json& v) { v["observation"]["manifest"]["track"]["arm"] = true; },
        [](json& v) { v["observation"]["manifest"]["clip"]["is_playing"] = true; },
        [](json& v) { v["observation"]["manifest"]["clip"]["looping"] = true; },
        [](json& v) { v["observation"]["manifest"]["clip"]["start_marker"] = 1.0; },
        [](json& v) { v["observation"]["manifest"]["mpe_note_expression_state_observed"] = true; },
        [](json& v) {
            v["observation"]["note_identity"]["entire_clip_population_observed"] = false;
        },
        [](json& v) { v["observation"]["note_identity"]["notes"][1]["note_id"] = 41; },
        [](json& v) { v["observation"]["note_identity"]["notes"][0]["velocity"] = 96; },
        [](json& v) { v["observation"]["note_identity"]["notes"][0]["pitch"] = 61; }};
    for (std::size_t index = 0; index < invalid.size(); ++index) {
        INFO(index);
        auto value = recovery_test_fixture::preview();
        invalid[index](value);
        sign_preview(value); // The forgery supplies internally recomputed hashes.
        CHECK_FALSE(managed_adoption_preview_from_json(value));
    }
}

TEST_CASE("Preserved foreign devices and envelopes do not confer destructive "
          "native authority",
          "[infrastructure][ableton][managed-recovery]") {
    auto value = recovery_test_fixture::preview();
    value["observation"]["manifest"]["devices_empty"] = false;
    // Unknown foreign devices have no invented finite owned-device supplement.
    value["observation"].erase("device_identity");
    value["observation"].erase("device_identity_fingerprint");
    value["observation"]["manifest"]["clip"]["has_envelopes"] = true;
    value["observation"]["structural_boundary_complete"] = false;
    sign_preview(value);
    const auto parsed = managed_adoption_preview_from_json(value);
    REQUIRE(parsed);
    CHECK(make_managed_adoption_request(context, "adopt_a", *parsed));
    CHECK_FALSE(make_managed_rebind_request(
        context, "rebind_a", {context, "project_a", "part_a", value.at("observation")}));
    CHECK(parsed->evidence.at("allowed_domains") ==
          json::array(
              {"existing_note_updates", "note_population_updates", "absent_mixer_step_lanes"}));
}

TEST_CASE("Adoption acknowledgement must reconstruct exact approved content "
          "and current IDs",
          "[infrastructure][ableton][managed-recovery]") {
    const std::vector<std::function<void(json&)>> invalid{
        [](json& v) { v["adoption"]["preview_token"] = std::string(32, 'b'); },
        [](json& v) { v["adoption"]["preview_fingerprint"] = std::string(64, 'b'); },
        [](json& v) { v["adoption"]["authority_origin"] = "created"; },
        [](json& v) { v["adoption"]["historical_identity_proven"] = true; },
        [](json& v) { v["adoption"]["devices_preserved"] = false; },
        [](json& v) { v["adoption"]["approved_note_ids"] = json::array({99, 41}); },
        [](json& v) { v["adoption"]["approved_note_ids"] = json::array({41, 41}); },
        [](json& v) { v["adoption"]["approved_note_ids"] = json::array({41}); },
        [](json& v) { v["adoption"]["preview_metadata"]["context"]["document_token"] = "foreign"; },
        [](json& v) { v["adoption"]["preview_metadata"]["project_key"] = "foreign"; },
        [](json& v) { v["adoption"]["preview_metadata"]["set_info"]["name"] = "Changed Set"; },
        [](json& v) { v["unsealed_authority"] = true; },
        [](json& v) {
            v["manifest"]["notes"][0]["velocity"] = 84.0;
            v["note_identity"]["notes"][0]["velocity"] = 84.0;
            sign_observation(v); // Valid actual hashes still differ from approved preview.
        },
        [](json& v) {
            v["note_identity"]["notes"][0]["note_id"] = 141;
            v["adoption"]["approved_note_ids"] = json::array({99, 141});
            sign_observation(v); // Same semantic notes do not preserve the approved current IDs.
        }};
    for (std::size_t index = 0; index < invalid.size(); ++index) {
        INFO(index);
        auto value = recovery_test_fixture::acknowledgement();
        invalid[index](value);
        CHECK_FALSE(managed_detail::adoption_acknowledgement_valid(intent(), value));
    }
}

TEST_CASE("Read-only preview response closes actual context and selected "
          "object locators",
          "[infrastructure][ableton][managed-recovery]") {
    Peer peer;
    peer.reply = recovery_test_fixture::preview();
    REQUIRE(preview_managed_adoption(context, "project_a", "part_a", selector, peer));
    REQUIRE(peer.requests.size() == 1);
    CHECK(peer.requests.front().property_or_method == "sunny_managed_preview_adoption");
    auto foreign = peer.reply;
    foreign["context"]["bridge_instance"] = "foreign";
    sign_preview(foreign);
    peer.reply = foreign;
    CHECK_FALSE(preview_managed_adoption(context, "project_a", "part_a", selector, peer));
    peer.reply = recovery_test_fixture::preview();
    CHECK_FALSE(preview_managed_adoption(context,
                                         "project_a",
                                         "part_a",
                                         {{"track_tag", "Sunny|project_a|part_a|track"},
                                          {"clip_tag", "Sunny|project_a|part_a|clip"}},
                                         peer));
    const auto parsed = managed_adoption_preview_from_json(recovery_test_fixture::preview());
    REQUIRE(parsed);
    auto corrupt = *parsed;
    corrupt.preview_token = std::string(32, 'b');
    CHECK_FALSE(make_managed_adoption_request(context, "adopt_a", corrupt));
    CHECK_FALSE(make_managed_adoption_request({"other_bridge", "document_a"}, "adopt_a", *parsed));
    CHECK(peer.requests.size() == 3); // Factories never issue an adoption dispatch.
}

TEST_CASE("Explicit Clip refresh seals a present owned-device supplement "
          "without granting new "
          "device authority",
          "[infrastructure][ableton][managed-recovery]") {
    auto value = recovery_test_fixture::owned_preview();
    const auto parsed = managed_adoption_preview_from_json(value);
    REQUIRE(parsed);
    CHECK(parsed->preview_fingerprint ==
          "314813954dac7716a3f62ebddc077cdcbdd16d98217e79120cb7dbe5c5587918");
    CHECK(parsed->evidence.at("observation")
              .at("device_identity")
              .at("cohort")
              .at(0)
              .at("device_key") == "source");
    CHECK(parsed->evidence.at("allowed_domains") ==
          json::array(
              {"existing_note_updates", "note_population_updates", "absent_mixer_step_lanes"}));
    const auto request = make_managed_adoption_request(context, "adopt_a", *parsed);
    REQUIRE(request);
    const auto payload = std::get<json>(request->args.at(0));
    CHECK(managed_detail::adoption_acknowledgement_valid(
        payload, recovery_test_fixture::owned_acknowledgement()));
    value.at("observation").erase("device_identity_fingerprint");
    sign_preview(value);
    CHECK_FALSE(managed_adoption_preview_from_json(value));
    value = recovery_test_fixture::owned_preview();
    value["observation"]["device_identity"]["opaque_state_observed"] = true;
    value["observation"]["device_identity_fingerprint"] =
        *managed_detail::managed_digest(value.at("observation").at("device_identity"));
    sign_preview(value);
    CHECK_FALSE(managed_adoption_preview_from_json(value));
    value = recovery_test_fixture::owned_preview();
    value["observation"]["device_identity"]["cohort"] = json::array();
    value["observation"]["device_identity_fingerprint"] =
        *managed_detail::managed_digest(value.at("observation").at("device_identity"));
    sign_preview(value);
    CHECK_FALSE(managed_adoption_preview_from_json(value));
    auto result = recovery_test_fixture::owned_acknowledgement();
    result["device_identity"]["cohort"][0]["parameters"][1]["descriptor"]["value"] = 0.5;
    result["device_identity_fingerprint"] =
        *managed_detail::managed_digest(result.at("device_identity"));
    CHECK_FALSE(managed_detail::adoption_acknowledgement_valid(payload, result));
}

TEST_CASE("Malformed adoption observation values return rejection without exceptions",
          "[infrastructure][ableton][managed-recovery]") {
    for (const auto& invalid : std::vector<json>{nullptr, 0, "object", json::array(), true}) {
        auto value = recovery_test_fixture::preview();
        value["observation"] = invalid;
        CHECK_FALSE(managed_adoption_preview_from_json(value));
    }
}

TEST_CASE("Current Clip adoption refuses a complete known reply above the "
          "bridge byte limit",
          "[infrastructure][ableton][managed-recovery]") {
    auto value = recovery_test_fixture::preview();
    auto& observation = value.at("observation");
    auto note = observation.at("manifest").at("notes").at(0);
    auto& notes = observation.at("manifest").at("notes");
    auto& identity = observation.at("note_identity").at("notes");
    notes = json::array();
    identity = json::array();
    for (std::int32_t index = 0; index < 26000; ++index) {
        note["start_time"] = static_cast<double>(index);
        notes.push_back(note);
        auto identified = note;
        identified["note_id"] = index + 1;
        identity.push_back(std::move(identified));
    }
    observation["manifest"]["clip"]["end_marker"] = 26001.0;
    observation["manifest"]["clip"]["loop_end"] = 26001.0;
    sign_preview(value);
    const auto preview = managed_adoption_preview_from_json(value);
    REQUIRE(preview);
    CHECK(preview->evidence.at("observation").at("note_identity").at("notes").size() == 26000);
    const auto request = make_managed_adoption_request(context, "adopt_a", *preview);
    REQUIRE_FALSE(request);
    CHECK(request.error() == sunny::core::ErrorCode::ManagedReplyCapacityExceeded);
}
