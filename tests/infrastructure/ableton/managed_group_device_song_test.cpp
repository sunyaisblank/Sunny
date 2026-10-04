#include "managed_device_fixture.hpp"
#include "managed_group_fixture.hpp"
#include "managed_song_settings_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <sunny/infrastructure/ableton/detail/managed_devices.hpp>
#include <sunny/infrastructure/ableton/detail/managed_group.hpp>
#include <sunny/infrastructure/ableton/detail/managed_song_settings.hpp>
#include <sunny/infrastructure/ableton/managed_devices.hpp>

using namespace sunny::infrastructure;
using nlohmann::json;
namespace {
const ManagedBridgeContext context{"bridge_a", "document_a"};
void grouped(json& observation, bool proof = true) {
    observation["manifest"]["track"]["is_grouped"] = true;
    observation["content_fingerprint"] =
        *managed_detail::managed_digest(observation.at("manifest"));
    observation["structural_boundary_complete"] = false;
    observation["content_boundary_complete"] = false;
    if (proof) {
        observation["group_authority"] =
            json::parse(MANAGED_GROUP_OBSERVATION).at("group_authority");
        auto& proof = observation["group_authority"];
        const auto index = observation.at("track_index").get<int>();
        proof["group_track_index"] = index == 0 ? 1 : 0;
        proof["member_track_indices"] = json::array({index});
        proof["selected_track_index"] = index;
        observation["group_authority_fingerprint"] = *managed_detail::managed_digest(proof);
    }
}
ManagedBindingReceipt binding(const json& observation) {
    return {context, "project_a", "part_a", observation};
}
void sign_group(json& observation) {
    observation["group_authority_fingerprint"] =
        *managed_detail::managed_digest(observation.at("group_authority"));
}
} // namespace

TEST_CASE("Native Device candidates require a current exact Group proof",
          "[ableton][managed-group-followup]") {
    auto observation = managed_device_fixture().at("before");
    grouped(observation, false);
    REQUIRE(managed_binding_from_json(managed_binding_to_json(binding(observation))));
    CHECK_FALSE(make_managed_device_insert_request(
        context, "insert", binding(observation), "source", ManagedNativeDevice::Drift, {}));
    CHECK_FALSE(make_managed_device_preview_request(context, binding(observation), {}));
    grouped(observation);
    REQUIRE(make_managed_device_insert_request(
        context, "insert", binding(observation), "source", ManagedNativeDevice::Drift, {}));
    REQUIRE(make_managed_device_preview_request(context, binding(observation), {}));
    observation["group_authority"]["context"]["bridge_instance"] = "bridge_b";
    sign_group(observation);
    CHECK_FALSE(make_managed_device_insert_request(
        context, "insert", binding(observation), "source", ManagedNativeDevice::Drift, {}));
}

TEST_CASE("Native Device ACK Group proof closes request and before after context",
          "[ableton][managed-group-followup]") {
    const auto fixture = managed_device_fixture();
    for (const auto* prefix : {"source", "effect", "update"}) {
        auto request = fixture.at(std::string(prefix) + "_request");
        auto result = fixture.at(std::string(prefix) + "_result");
        auto& before = result["device_update"]["before_observation"];
        grouped(before);
        grouped(result);
        request["expected_content_fingerprint"] = before.at("content_fingerprint");
        const auto method = std::string_view(prefix) == "update"
                                ? "sunny_managed_update_device_parameters"
                                : "sunny_managed_insert_device";
        REQUIRE(managed_device_detail::device_result_matches_request(method, request, result));
        auto bad = result;
        bad["device_update"]["before_observation"].erase("group_authority");
        bad["device_update"]["before_observation"].erase("group_authority_fingerprint");
        CHECK_FALSE(managed_device_detail::device_result_matches_request(method, request, bad));
        bad = result;
        auto& wrong = bad["device_update"]["before_observation"];
        wrong["group_authority"]["context"]["bridge_instance"] = "bridge_b";
        sign_group(wrong);
        CHECK_FALSE(managed_device_detail::device_result_matches_request(method, request, bad));
        bad = result;
        bad["group_authority"]["context"]["document_token"] = "document_b";
        sign_group(bad);
        CHECK_FALSE(managed_device_detail::device_result_matches_request(method, request, bad));
    }
    ManagedDeviceAdoptionPreview malformed{context, "project_a", "part_a", "preview", "", {}};
    CHECK_NOTHROW(make_managed_device_adoption_request(context, "adopt", malformed));
    CHECK_FALSE(make_managed_device_adoption_request(context, "adopt", malformed));
}

TEST_CASE("Setwide Song candidate cannot bypass the selected Part Group proof",
          "[ableton][managed-group-followup]") {
    auto response = song_settings_test_fixture::normal_preview();
    auto& observation = response["observation"];
    grouped(observation, false);
    REQUIRE(managed_binding_from_json(managed_binding_to_json(binding(observation))));
    CHECK_FALSE(
        make_managed_song_settings_preview_request(context, binding(observation), {92.5, 3, 8}));
    grouped(observation);
    response["preview"]["binding_guard"]["content_fingerprint"] =
        observation.at("content_fingerprint");
    response["preview_fingerprint"] = *managed_detail::managed_digest(response.at("preview"));
    const auto request =
        make_managed_song_settings_preview_request(context, binding(observation), {92.5, 3, 8});
    REQUIRE(request);
    const auto preview = parse_managed_song_settings_preview(*request, context, response);
    REQUIRE(preview);
    REQUIRE(make_managed_song_settings_request(context, "song", *preview));
    response["observation"].erase("group_authority");
    response["observation"].erase("group_authority_fingerprint");
    CHECK_FALSE(parse_managed_song_settings_preview(*request, context, response));
}

TEST_CASE("Setwide Song ACK uses current Group context without granting completeness",
          "[ableton][managed-group-followup]") {
    auto response = song_settings_test_fixture::normal_preview();
    grouped(response["observation"]);
    response["preview"]["binding_guard"]["content_fingerprint"] =
        response.at("observation").at("content_fingerprint");
    response["preview_fingerprint"] = *managed_detail::managed_digest(response.at("preview"));
    const auto query = make_managed_song_settings_preview_request(
        context, binding(response.at("observation")), {92.5, 3, 8});
    REQUIRE(query);
    const auto preview = parse_managed_song_settings_preview(*query, context, response);
    REQUIRE(preview);
    const auto operation = make_managed_song_settings_request(context, "song_a", *preview);
    REQUIRE(operation);
    const auto request = std::get<json>(operation->args[0]);
    auto result = song_settings_test_fixture::normal_result();
    grouped(result);
    result["song_settings"]["approved_preview"] = response.at("preview");
    result["song_settings"]["preview_fingerprint"] = response.at("preview_fingerprint");
    REQUIRE(managed_song_detail::result_matches_request(request, result));
    REQUIRE(managed_song_detail::acknowledged_native_start_valid(request, result, true));
    CHECK(result.at("content_boundary_complete") == false);
    result.erase("group_authority");
    result.erase("group_authority_fingerprint");
    CHECK_FALSE(managed_song_detail::result_matches_request(request, result));
}
