#include "managed_device_modes_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <sunny/infrastructure/ableton/detail/managed_devices.hpp>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/managed_devices.hpp>

using namespace sunny::infrastructure;
using nlohmann::json;
namespace {
void rehash(json& value) {
    value["device_identity_fingerprint"] =
        *managed_detail::managed_digest(value.at("device_identity"));
}
} // namespace

TEST_CASE("Native mode ACK proves actual permuted labels native properties and unchanged notes",
          "[ableton][managed-device-mode]") {
    const auto fixture = managed_device_modes_fixture();
    CHECK(managed_device_detail::device_request_valid("sunny_managed_update_device_modes",
                                                      fixture.at("mode_request")));
    CHECK(managed_device_detail::device_mode_result_matches_request(fixture.at("mode_request"),
                                                                    fixture.at("mode_result")));
    CHECK(managed_device_detail::device_result_matches_request("sunny_managed_update_device_modes",
                                                               fixture.at("mode_request"),
                                                               fixture.at("mode_result")));
    CHECK(fixture.at("mode_result")
              .at("device_mode_update")
              .at("readbacks")[1]
              .at("internal_value") == 3.0);
    CHECK(fixture.at("mode_result")
              .at("device_identity")
              .at("cohort")[1]
              .at("modes")
              .at("global_mode") == 0);
    CHECK(managed_device_detail::device_result_matches_request(
        "sunny_managed_update_device_parameters",
        fixture.at("scale_request"),
        fixture.at("scale_result")));
    CHECK(fixture.at("scale_result").at("device_update").at("readbacks")[0].at("display") ==
          "100.00 %");
}

TEST_CASE(
    "Rehashed mode ACK cannot invent an ordinal change untouched value or broaden another Device",
    "[ableton][managed-device-mode][closure]") {
    const auto fixture = managed_device_modes_fixture();
    auto wrong = fixture.at("mode_result");
    wrong["device_mode_update"]["admitted_modes"][1]["target_internal"] = 0.0;
    wrong["device_mode_update"]["readbacks"][1]["internal_value"] = 0.0;
    wrong["device_identity"]["cohort"][1]["parameters"][5]["descriptor"]["value"] = 0.0;
    rehash(wrong);
    CHECK_FALSE(managed_device_detail::device_mode_result_matches_request(
        fixture.at("mode_request"), wrong));
    wrong = fixture.at("mode_result");
    wrong["device_identity"]["cohort"][1]["parameters"][7]["descriptor"]["value"] = 0.25;
    rehash(wrong);
    CHECK_FALSE(managed_device_detail::device_mode_result_matches_request(
        fixture.at("mode_request"), wrong));
    wrong = fixture.at("mode_result");
    wrong["device_identity"]["cohort"][0]["parameters"][1]["descriptor"]["state"] = 1;
    rehash(wrong);
    CHECK_FALSE(managed_device_detail::device_mode_result_matches_request(
        fixture.at("mode_request"), wrong));
    wrong = fixture.at("mode_result");
    wrong["device_identity"]["cohort"][1]["modes"]["oversample"] = true;
    rehash(wrong);
    CHECK_FALSE(managed_device_detail::device_mode_result_matches_request(
        fixture.at("mode_request"), wrong));
}

TEST_CASE(
    "Native effect bypass proof is exact and historical EQ property shape remains honest unknown",
    "[ableton][managed-device-mode][bypass]") {
    const auto fixture = managed_device_modes_fixture();
    CHECK(managed_device_detail::device_mode_result_matches_request(fixture.at("bypass_request"),
                                                                    fixture.at("bypass_result")));
    auto identity = fixture.at("bypass_result").at("device_identity");
    CHECK(managed_device_detail::device_identity_valid(identity));
    identity["cohort"][1]["parameters"][0]["descriptor"]["value_items"][0] = "Unknown";
    CHECK_FALSE(managed_device_detail::device_identity_valid(identity));
    identity = fixture.at("mode_result").at("device_identity");
    identity["cohort"][1]["modes"].erase("edit_mode");
    identity["cohort"][1]["modes"].erase("oversample");
    CHECK(managed_device_detail::device_identity_valid(identity));
    identity["cohort"][1]["modes"]["edit_mode"] = false;
    CHECK_FALSE(managed_device_detail::device_identity_valid(identity));
    identity = fixture.at("mode_result").at("device_identity");
    identity["cohort"][1]["modes"]["oversample"] = 0;
    CHECK_FALSE(managed_device_detail::device_identity_valid(identity));
}

TEST_CASE("Native mode request is finite closed and uses actual property names",
          "[ableton][managed-device-mode][request]") {
    const auto fixture = managed_device_modes_fixture();
    for (const auto* property : {"Eq Mode", "oversample", "__dict__"}) {
        auto wrong = fixture.at("mode_request");
        wrong["property_intents"][0]["property"] = property;
        CHECK_FALSE(managed_device_detail::device_request_valid("sunny_managed_update_device_modes",
                                                                wrong));
    }
    auto wrong = fixture.at("mode_request");
    wrong["enum_intents"][0]["internal_value"] = 0.0;
    CHECK_FALSE(
        managed_device_detail::device_request_valid("sunny_managed_update_device_modes", wrong));
    wrong = fixture.at("mode_request");
    wrong["enum_intents"].push_back(wrong.at("enum_intents")[0]);
    CHECK_FALSE(
        managed_device_detail::device_request_valid("sunny_managed_update_device_modes", wrong));
    wrong = fixture.at("mode_request");
    wrong["physical_intents"] = json::array();
    CHECK_FALSE(
        managed_device_detail::device_request_valid("sunny_managed_update_device_modes", wrong));
}

TEST_CASE("Explicit inactive effect adoption proves current Off mode without physical evidence",
          "[ableton][managed-device-mode][adoption]") {
    const auto fixture = managed_device_modes_fixture();
    CHECK(managed_device_detail::device_request_valid("sunny_managed_preview_devices",
                                                      fixture.at("bypass_preview_request")));
    CHECK(managed_device_detail::device_preview_valid(fixture.at("bypass_preview")));
    CHECK(managed_device_detail::device_adoption_result_matches_request(
        fixture.at("bypass_adoption_request"), fixture.at("bypass_adoption_result")));
    CHECK(fixture.at("bypass_preview").at("resolutions").empty());
    auto wrong = fixture.at("bypass_preview");
    wrong["devices"][1]["enum_intents"][0]["label"] = "On";
    CHECK_FALSE(managed_device_detail::device_preview_valid(wrong));
    wrong = fixture.at("bypass_preview");
    wrong["devices"][1]["authored_bypass"] = false;
    CHECK_FALSE(managed_device_detail::device_preview_valid(wrong));
    wrong = fixture.at("bypass_preview_request");
    wrong["devices"][1]["physical_intents"].push_back(
        {{"capability_id", "utility.gain"}, {"target", 0.0}, {"tolerance", 0.0}});
    CHECK_FALSE(
        managed_device_detail::device_request_valid("sunny_managed_preview_devices", wrong));
}
