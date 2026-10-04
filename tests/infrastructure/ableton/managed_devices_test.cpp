#include "managed_device_decimal_fixture.hpp"
#include "managed_device_fixture.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/infrastructure/ableton/detail/managed_devices.hpp>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/managed_devices.hpp>

using namespace sunny::infrastructure;
using nlohmann::json;

namespace {
void refresh_device_digest(json& observation) {
    const auto digest = managed_detail::managed_digest(observation.at("device_identity"));
    REQUIRE(digest);
    observation["device_identity_fingerprint"] = *digest;
}
ManagedBindingReceipt binding(const json& observation) {
    return {{"bridge_a", "document_a"}, "project_a", "part_a", observation};
}
} // namespace

TEST_CASE("Managed device literal native ACK preserves notes while creating "
          "audio output",
          "[ableton][managed-device][join]") {
    const auto fixture = managed_device_fixture();
    const auto& result = fixture.at("source_result");
    REQUIRE(managed_device_detail::device_result_matches_request(
        "sunny_managed_insert_device", fixture.at("source_request"), result));
    CHECK(result.at("manifest").at("track").at("has_audio_output") == true);
    CHECK(result.at("note_identity") == fixture.at("before").at("note_identity"));
    CHECK(result.at("device_update").at("readbacks")[0].at("internal_value") == .5);
    CHECK(result.at("device_update").at("readbacks")[0].at("display") == "1200.00 Hz");
    CHECK(result.at("device_identity").at("opaque_state_observed") == false);
    CHECK(managed_device_detail::device_result_matches_request(
        "sunny_managed_insert_device", fixture.at("effect_request"), fixture.at("effect_result")));
    CHECK(managed_device_detail::device_result_matches_request(
        "sunny_managed_update_device_parameters",
        fixture.at("update_request"),
        fixture.at("update_result")));
}

TEST_CASE("Managed native readback keeps exact decimal tolerance without "
          "accepting rounded outside text",
          "[ableton][managed-device][decimal]") {
    const auto fixture = managed_device_decimal_fixture();
    const auto& request = fixture.at("request");
    auto result = fixture.at("result");
    const auto method = "sunny_managed_update_device_parameters";
    REQUIRE(result.at("device_update").at("readbacks")[1].at("display") == "100.01 %");
    REQUIRE(managed_device_detail::device_result_matches_request(method, request, result));
    auto& readback = result["device_update"]["readbacks"][1];
    readback["display"] = "100.0100000000000000001 %";
    readback["display_increment"] = 1e-19;
    // Producer flags and binary display/error values cannot prove this scope.
    CHECK(readback.at("display_value") == 100.01);
    CHECK(readback.at("absolute_display_error") == .01);
    CHECK(readback.at("matches_intent") == true);
    CHECK_FALSE(managed_device_detail::device_result_matches_request(method, request, result));
    result = fixture.at("result");
    result["device_update"]["readbacks"][1]["absolute_display_error"] = 0.0;
    CHECK_FALSE(managed_device_detail::device_result_matches_request(method, request, result));
}

TEST_CASE("Managed device typed request preserves exact finite targets and "
          "independent guards",
          "[ableton][managed-device][request]") {
    const auto fixture = managed_device_fixture();
    const std::array intents{ManagedDevicePhysicalIntent{"drift.lp.frequency", 1200.0, 0.0}};
    const auto prepared = make_managed_device_insert_request({"bridge_a", "document_a"},
                                                             "source",
                                                             binding(fixture.at("before")),
                                                             "source",
                                                             ManagedNativeDevice::Drift,
                                                             intents);
    REQUIRE(prepared);
    CHECK(prepared->property_or_method == "sunny_managed_insert_device");
    CHECK(std::get<json>(prepared->args[0]) == fixture.at("source_request"));
    const std::array utility{ManagedDevicePhysicalIntent{"utility.gain", 0.0, 0.0}};
    const auto update = make_managed_device_update_request({"bridge_a", "document_a"},
                                                           "gain_update",
                                                           binding(fixture.at("effect_result")),
                                                           "effect",
                                                           ManagedNativeDevice::Utility,
                                                           utility);
    REQUIRE(update);
    CHECK(std::get<json>(update->args[0]) == fixture.at("update_request"));
    CHECK_FALSE(make_managed_device_insert_request({"bridge_a", "document_a"},
                                                   "bad",
                                                   binding(fixture.at("before")),
                                                   "effect",
                                                   ManagedNativeDevice::Utility,
                                                   utility));
    CHECK_FALSE(make_managed_device_update_request({"bridge_a", "document_a"},
                                                   "bad",
                                                   binding(fixture.at("source_result")),
                                                   "missing",
                                                   ManagedNativeDevice::Utility,
                                                   utility));
}

TEST_CASE("Managed device result independently proves formatted readback "
          "instead of trusting flags",
          "[ableton][managed-device][decline]") {
    const auto fixture = managed_device_fixture();
    for (const auto& field : {"display_value", "absolute_display_error", "internal_value"}) {
        auto result = fixture.at("source_result");
        result["device_update"]["readbacks"][0][field] = 123.0;
        CHECK_FALSE(managed_device_detail::device_result_matches_request(
            "sunny_managed_insert_device", fixture.at("source_request"), result));
    }
    for (const auto& display : {"1,2 kHz", "1200", "1200 ms", "1.3 kHz"}) {
        auto result = fixture.at("source_result");
        result["device_update"]["readbacks"][0]["display"] = display;
        CHECK_FALSE(managed_device_detail::device_result_matches_request(
            "sunny_managed_insert_device", fixture.at("source_request"), result));
    }
}

TEST_CASE("Managed device join detects unselected native values and note "
          "identity changes with "
          "valid digests",
          "[ableton][managed-device][decline]") {
    const auto fixture = managed_device_fixture();
    auto result = fixture.at("update_result");
    result["device_identity"]["cohort"][0]["parameters"][3]["descriptor"]["value"] = .125;
    refresh_device_digest(result);
    CHECK(managed_device_detail::device_supplement_valid(result));
    CHECK_FALSE(managed_device_detail::device_result_matches_request(
        "sunny_managed_update_device_parameters", fixture.at("update_request"), result));
    result = fixture.at("source_result");
    result["note_identity"]["notes"][0]["note_id"] = 77;
    const auto digest = managed_detail::managed_digest(result.at("note_identity"));
    REQUIRE(digest);
    result["note_identity_fingerprint"] = *digest;
    CHECK_FALSE(managed_device_detail::device_result_matches_request(
        "sunny_managed_insert_device", fixture.at("source_request"), result));
}

TEST_CASE("Managed device closed schema declines descriptors duplicate targets "
          "and unknown roles",
          "[ableton][managed-device][decline]") {
    const auto fixture = managed_device_fixture();
    auto request = fixture.at("source_request");
    request["native_parameter_index"] = 1;
    CHECK_FALSE(
        managed_device_detail::device_request_valid("sunny_managed_insert_device", request));
    request = fixture.at("source_request");
    request["physical_intents"].push_back(request.at("physical_intents")[0]);
    CHECK_FALSE(
        managed_device_detail::device_request_valid("sunny_managed_insert_device", request));
    request = fixture.at("source_request");
    request["device"]["class_name"] = "Operator";
    CHECK_FALSE(
        managed_device_detail::device_request_valid("sunny_managed_insert_device", request));
    request = fixture.at("source_request");
    request["physical_intents"][0]["target"] = std::numeric_limits<double>::infinity();
    CHECK_FALSE(
        managed_device_detail::device_request_valid("sunny_managed_insert_device", request));
    auto result = fixture.at("source_result");
    result.erase("device_identity_fingerprint");
    CHECK_FALSE(managed_device_detail::device_supplement_valid(result));
}

TEST_CASE("Managed device current preview and adoption retain literal "
          "formatted intent without writes",
          "[ableton][managed-device][adoption]") {
    const auto fixture = managed_device_fixture();
    const std::array selections{
        ManagedDeviceAdoptionSelection{
            "current_source", 0, ManagedNativeDevice::Drift, {{"drift.lp.frequency", 1200.0, 0.0}}},
        ManagedDeviceAdoptionSelection{
            "current_effect", 1, ManagedNativeDevice::Utility, {{"utility.gain", 0.0, 0.0}}}};
    const ManagedBridgeContext context{"bridge_a", "document_a"};
    const auto query = make_managed_device_preview_request(
        context, binding(fixture.at("update_result")), selections);
    REQUIRE(query);
    CHECK(std::get<json>(query->args[0]) == fixture.at("preview_request"));
    const auto preview =
        parse_managed_device_preview(*query, context, fixture.at("preview_response"));
    REQUIRE(preview);
    CHECK(preview->approved_preview.at("resolutions")[0].at("current_readback").at("display") ==
          "1200.00 Hz");
    CHECK(preview->approved_preview.at("resolutions")[0]
              .at("current_readback")
              .at("internal_value") == .5);
    const auto adoption = make_managed_device_adoption_request(context, "adopt_devices", *preview);
    REQUIRE(adoption);
    CHECK(std::get<json>(adoption->args[0]) == fixture.at("adopt_request"));
    REQUIRE(managed_device_detail::device_adoption_result_matches_request(
        fixture.at("adopt_request"), fixture.at("adopt_result")));
    CHECK(fixture.at("adopt_result").at("device_adoption").at("native_mutation_started") == false);
    CHECK(fixture.at("adopt_result").at("note_identity") ==
          fixture.at("update_result").at("note_identity"));
    CHECK_FALSE(managed_device_detail::device_result_matches_request(
        "sunny_managed_adopt_devices", fixture.at("adopt_request"), fixture.at("adopt_result")));
}

TEST_CASE("Managed device adoption rejects mismatched current formatter and "
          "rewritten after state",
          "[ableton][managed-device][adoption][decline]") {
    const auto fixture = managed_device_fixture();
    for (const auto& display : {"1200", "1,2 kHz", "1.3 kHz", "1200 ms"}) {
        auto preview = fixture.at("preview_response").at("preview");
        preview["resolutions"][0]["current_readback"]["display"] = display;
        CHECK_FALSE(managed_device_detail::device_preview_valid(preview));
    }
    auto preview = fixture.at("preview_response").at("preview");
    preview["resolutions"][0]["current_readback"]["internal_value"] = 0.0;
    CHECK_FALSE(managed_device_detail::device_preview_valid(preview));
    auto result = fixture.at("adopt_result");
    result["device_identity"]["cohort"][0]["parameters"][1]["descriptor"]["value"] = .75;
    refresh_device_digest(result);
    REQUIRE(managed_device_detail::device_supplement_valid(result));
    CHECK_FALSE(managed_device_detail::device_adoption_result_matches_request(
        fixture.at("adopt_request"), result));
    result = fixture.at("adopt_result");
    result["device_adoption"]["native_mutation_started"] = true;
    CHECK_FALSE(managed_device_detail::device_adoption_result_matches_request(
        fixture.at("adopt_request"), result));
    CHECK_FALSE(managed_device_detail::device_preview_valid(nullptr));
    CHECK_FALSE(managed_device_detail::device_adoption_result_matches_request(nullptr, nullptr));
}

TEST_CASE("Managed device supplement cannot claim an empty manifest against an "
          "owned chain",
          "[ableton][managed-device][decline]") {
    const auto fixture = managed_device_fixture();
    auto result = fixture.at("source_result");
    result["manifest"]["devices_empty"] = true;
    const auto digest = managed_detail::managed_digest(result.at("manifest"));
    REQUIRE(digest);
    result["content_fingerprint"] = *digest;
    CHECK_FALSE(managed_device_detail::device_supplement_valid(result));
    auto request = fixture.at("preview_request");
    request["devices"][0]["device"]["class_name"] = "PluginDevice";
    CHECK_FALSE(
        managed_device_detail::device_request_valid("sunny_managed_preview_devices", request));
    request = fixture.at("preview_request");
    request["devices"][1]["chain_index"] = 0;
    CHECK_FALSE(
        managed_device_detail::device_request_valid("sunny_managed_preview_devices", request));
}

TEST_CASE("Managed device adoption capacity declines before a durable "
          "operation fence",
          "[ableton][managed-device][capacity]") {
    const auto fixture = managed_device_fixture();
    auto response = fixture.at("preview_response");
    auto& observation = response["preview"]["binding_observation"];
    // Three observed unselected enum populations fit the finite capture domain
    // and SM1's16MiB binary UTF-8 digest, but their escaped JSON reply exceeds
    // the wire frame. This avoids conflating canonical bytes with wire bytes.
    std::string astral_label;
    for (int point = 0; point < 1024; ++point)
        astral_label += "\xF4\x8F\xBF\xBF";
    auto labels = json::array();
    for (int label = 0; label < 512; ++label)
        labels.push_back(astral_label);
    for (const auto index : {0, 2, 3}) {
        auto& descriptor =
            observation["device_identity"]["cohort"][0]["parameters"][index]["descriptor"];
        descriptor["is_quantized"] = true;
        descriptor["default_value"] = nullptr;
        descriptor["value_items"] = labels;
    }
    refresh_device_digest(observation);
    REQUIRE(managed_device_detail::device_preview_valid(response.at("preview")));
    ManagedDeviceAdoptionPreview preview{{"bridge_a", "document_a"},
                                         "project_a",
                                         "part_a",
                                         response.at("preview_token"),
                                         *managed_detail::managed_digest(response.at("preview")),
                                         response.at("preview")};
    const auto request = make_managed_device_adoption_request(preview.context, "oversize", preview);
    REQUIRE_FALSE(request);
    CHECK(request.error() == sunny::core::ErrorCode::ManagedReplyCapacityExceeded);
}

TEST_CASE("Managed device explicitly empty current chain permits prospective "
          "append authority",
          "[ableton][managed-device][adoption][empty]") {
    const auto fixture = managed_device_fixture();
    const ManagedBridgeContext context{"bridge_a", "document_a"};
    const std::array<ManagedDeviceAdoptionSelection, 0> none{};
    const auto query =
        make_managed_device_preview_request(context, binding(fixture.at("before")), none);
    REQUIRE(query);
    CHECK(std::get<json>(query->args[0]) == fixture.at("empty_preview_request"));
    const auto preview =
        parse_managed_device_preview(*query, context, fixture.at("empty_preview_response"));
    REQUIRE(preview);
    CHECK(preview->approved_preview.at("resolutions").empty());
    CHECK(preview->approved_preview.at("binding_observation")
              .at("device_identity")
              .at("cohort")
              .empty());
    const auto adoption = make_managed_device_adoption_request(context, "adopt_empty", *preview);
    REQUIRE(adoption);
    CHECK(std::get<json>(adoption->args[0]) == fixture.at("empty_adopt_request"));
    CHECK(managed_device_detail::device_adoption_result_matches_request(
        fixture.at("empty_adopt_request"), fixture.at("empty_adopt_result")));
    const std::array cutoff{ManagedDevicePhysicalIntent{"drift.lp.frequency", 1200.0, 0.0}};
    CHECK(make_managed_device_insert_request(context,
                                             "source",
                                             binding(fixture.at("empty_adopt_result")),
                                             "source",
                                             ManagedNativeDevice::Drift,
                                             cutoff));
    auto forged_ack = fixture.at("source_result");
    forged_ack["device_adoption"] = fixture.at("empty_adopt_result").at("device_adoption");
    REQUIRE(managed_device_detail::device_supplement_valid(forged_ack));
    CHECK_FALSE(managed_device_detail::device_adoption_result_matches_request(
        fixture.at("empty_adopt_request"), forged_ack));
}

TEST_CASE("Managed native descriptor types remain exact after a valid SM1 rehash",
          "[ableton][managed-device][decline][numeric-types]") {
    const auto fixture = managed_device_fixture();
    for (const auto* field : {"minimum", "maximum", "value", "default_value"}) {
        auto result = fixture.at("source_result");
        auto& descriptor = result["device_identity"]["cohort"][0]["parameters"][2]["descriptor"];
        const double literal = descriptor.at(field);
        REQUIRE(std::trunc(literal) == literal);
        descriptor[field] = static_cast<int>(literal);
        refresh_device_digest(result);
        CHECK_FALSE(managed_device_detail::device_supplement_valid(result));
        CHECK_FALSE(managed_device_detail::device_result_matches_request(
            "sunny_managed_insert_device", fixture.at("source_request"), result));
        result = fixture.at("source_result");
        auto& candidate_descriptor =
            result["device_update"]["resolutions"][0]["candidate"]["descriptor"];
        const double candidate_literal = candidate_descriptor.at(field);
        REQUIRE(std::trunc(candidate_literal) == candidate_literal);
        candidate_descriptor[field] = static_cast<int>(candidate_literal);
        CHECK_FALSE(managed_device_detail::device_result_matches_request(
            "sunny_managed_insert_device", fixture.at("source_request"), result));
    }
    auto result = fixture.at("source_result");
    result["device_identity"]["schema_version"] = 1.0;
    refresh_device_digest(result);
    CHECK_FALSE(managed_device_detail::device_supplement_valid(result));
}

TEST_CASE("Managed native quantized default is explicitly unavailable while "
          "continuous default is "
          "observed",
          "[ableton][managed-device][conditional-domain]") {
    const auto fixture = managed_device_fixture();
    const auto& result = fixture.at("source_result");
    REQUIRE(managed_device_detail::device_supplement_valid(result));
    CHECK(result.at("device_identity")
              .at("cohort")[0]
              .at("parameters")[0]
              .at("descriptor")
              .at("default_value")
              .is_null());
    CHECK(result.at("device_identity")
              .at("cohort")[0]
              .at("parameters")[1]
              .at("descriptor")
              .at("default_value") == 0.0);
    auto wrong = result;
    wrong["device_identity"]["cohort"][0]["parameters"][0]["descriptor"]["default_value"] = 1.0;
    refresh_device_digest(wrong);
    CHECK_FALSE(managed_device_detail::device_supplement_valid(wrong));
    wrong = result;
    wrong["device_identity"]["cohort"][0]["parameters"][1]["descriptor"]["default_value"] = nullptr;
    refresh_device_digest(wrong);
    CHECK_FALSE(managed_device_detail::device_supplement_valid(wrong));
}

#include "managed_inspection_fixture.hpp"

#include <sunny/infrastructure/ableton/deployment.hpp>

TEST_CASE("Read-only device inspection parses actual nonlinear formatter and ordered modes",
          "[ableton][managed-device][inspection]") {
    const auto fixture = test::managed_inspection_fixture();
    const auto& actual = fixture.at("device_response");
    const ManagedBridgeContext context{"bridge_a", "document_a"};
    const std::array selections{
        ManagedDeviceAdoptionSelection{
            "source", 0, ManagedNativeDevice::Drift, {{"drift.lp.frequency", 1200.0, 0.0}}},
        ManagedDeviceAdoptionSelection{"effect",
                                       1,
                                       ManagedNativeDevice::Utility,
                                       {{"utility.gain", 0.0, 0.0}},
                                       {{"utility.channel_mode", "Stereo"}}}};
    const auto request = make_managed_device_inspection_request(
        context, binding(actual.at("inspection").at("binding_observation")), selections);
    REQUIRE(request);
    CHECK(request->property_or_method == "sunny_managed_inspect_devices");
    CHECK(std::get<json>(request->args[0]) == fixture.at("device_request"));
    REQUIRE(
        LomProtocol::deserialize_request(json::parse(LomProtocol::serialize_request(*request))));
    const auto observed = parse_managed_device_inspection(*request, context, actual);
    REQUIRE(observed);
    CHECK(*observed == actual);
    CHECK(actual.at("inspection").at("authority_origin") == "none");
    CHECK(actual.at("inspection").at("resolutions")[0].at("current_readback").at("display") ==
          "1200.00 Hz");
    CHECK(actual.at("inspection").at("resolutions")[1].at("current_readback").at("display") ==
          "0.00 dB");
    CHECK_FALSE(parse_managed_device_preview(*request, context, actual));
    CHECK_FALSE(prepare_managed_operation(context, *request));
    CommandBuffer peer;
    JournaledLomTransport readonly{peer, std::nullopt, {}, true};
    CHECK(readonly.send(*request).success);
    CHECK(readonly.journal().empty());
    CHECK_FALSE(readonly.plan_diverged());
    REQUIRE(peer.entries().size() == 1);
    CHECK(peer.entries()[0].request.property_or_method == "sunny_managed_inspect_devices");
}

TEST_CASE("Read-only device inspection independently closes fields guards physical text and modes",
          "[ableton][managed-device][inspection][decline]") {
    const auto fixture = test::managed_inspection_fixture();
    const ManagedBridgeContext context{"bridge_a", "document_a"};
    const auto request = LomProtocol::call_method(
        LomPaths::song(), "sunny_managed_inspect_devices", {fixture.at("device_request")});
    const auto original = fixture.at("device_response");
    for (const auto& field : {"document_token", "project_key", "binding_key"}) {
        auto response = original;
        response[field] = "foreign";
        CHECK_FALSE(parse_managed_device_inspection(request, context, response));
    }
    for (const auto& value : {json(0), json(nullptr), json(true)}) {
        auto response = original;
        response["inspection"]["native_mutation_started"] = value;
        CHECK_FALSE(parse_managed_device_inspection(request, context, response));
    }
    auto response = original;
    response["inspection"]["authority_origin"] = "explicit_current_device_adoption";
    CHECK_FALSE(parse_managed_device_inspection(request, context, response));
    response = original;
    response["preview_token"] = std::string(32, '0');
    CHECK_FALSE(parse_managed_device_inspection(request, context, response));
    response = original;
    response["inspection"]["resolutions"][0]["current_readback"]["display"] = "1300.00 Hz";
    CHECK_FALSE(parse_managed_device_inspection(request, context, response));
    response = original;
    auto& observation = response["inspection"]["binding_observation"];
    observation["device_identity"]["cohort"][1]["parameters"][4]["descriptor"]["value"] = 0.0;
    refresh_device_digest(observation);
    REQUIRE(managed_device_detail::device_supplement_valid(observation));
    CHECK_FALSE(parse_managed_device_inspection(request, context, response));
    response = original;
    response["inspection"]["binding_observation"]["track_tag"] = "Sunny|foreign|part_a|track";
    CHECK_FALSE(parse_managed_device_inspection(request, context, response));
}
