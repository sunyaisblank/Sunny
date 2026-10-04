#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <limits>
#include <sunny/infrastructure/ableton/deployment.hpp>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/managed_realization.hpp>

using namespace sunny::infrastructure;
using nlohmann::json;

namespace {
const ManagedBridgeContext context{"bridge_a", "document_a"};

ManagedClipProjection projection() {
    ManagedClipProjection result;
    result.clip_end = 4.0;
    result.notes = json::array({{{"pitch", 60},
                                 {"start_time", 0.0},
                                 {"duration", 1.0},
                                 {"velocity", 96},
                                 {"mute", false},
                                 {"probability", 1.0},
                                 {"velocity_deviation", 0.0},
                                 {"release_velocity", 64.0}}});
    return result;
}

json observation() {
    // Literal actual Python model acknowledgement, with typed SM1 SHA256.
    return json::parse(R"JSON({
  "track_index": 1,
  "slot_index": 0,
  "manifest": {
    "schema_version": 1,
    "track": {
      "name": "Sunny|project_a|part_a|track",
      "mute": false,
      "solo": false,
      "arm": false,
      "implicit_arm": false,
      "is_frozen": false,
      "is_grouped": false,
      "back_to_arranger": false,
      "has_audio_input": false,
      "has_midi_input": true,
      "has_audio_output": false,
      "has_midi_output": true
    },
    "clip": {
      "name": "Sunny|project_a|part_a|clip",
      "signature_numerator": 4,
      "signature_denominator": 4,
      "start_marker": 0.0,
      "end_marker": 4.0,
      "loop_start": 0.0,
      "loop_end": 4.0,
      "looping": false,
      "muted": false,
      "has_envelopes": false,
      "has_groove": false,
      "is_session_clip": true,
      "is_arrangement_clip": false,
      "is_midi_clip": true,
      "is_audio_clip": false,
      "is_playing": false,
      "is_recording": false,
      "is_overdubbing": false,
      "is_triggered": false,
      "will_record_on_start": false,
      "launch_mode": 0,
      "launch_quantization": 1,
      "legato": false,
      "velocity_amount": 0.0
    },
    "notes": [
      {
        "pitch": 60,
        "start_time": 0.0,
        "duration": 1.0,
        "velocity": 96.0,
        "mute": false,
        "probability": 1.0,
        "velocity_deviation": 0.0,
        "release_velocity": 64.0
      }
    ],
    "mixer": {
      "panning_mode": 0,
      "crossfade_assign": 1,
      "volume": {
        "name": "Track Volume",
        "original_name": "Track Volume",
        "value": 0.85,
        "min": 0.0,
        "max": 1.0,
        "is_quantized": false,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "panning": {
        "name": "Track Panning",
        "original_name": "Track Panning",
        "value": 0.0,
        "min": -1.0,
        "max": 1.0,
        "is_quantized": false,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "track_activator": {
        "name": "Speaker On",
        "original_name": "Speaker On",
        "value": 1.0,
        "min": 0.0,
        "max": 1.0,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "sends": [
        {
          "name": "Send A",
          "original_name": "Send A",
          "value": 0.0,
          "min": 0.0,
          "max": 1.0,
          "is_quantized": false,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0
        }
      ]
    },
    "routing": {
      "input_routing_type": {
        "display_name": "All Ins",
        "identifier": "0:All Ins"
      },
      "input_routing_channel": {
        "display_name": "All Channels",
        "identifier": "All Channels"
      },
      "output_routing_type": {
        "display_name": "No Output",
        "identifier": "5:No Output"
      },
      "output_routing_channel": {
        "display_name": "",
        "identifier": ""
      }
    },
    "content_counts": {
      "arrangement_clips": 0,
      "take_lanes": 0
    },
    "devices_empty": true,
    "other_session_clips_empty": true,
    "entire_clip_population_observed": true,
    "mpe_note_expression_state_observed": false,
    "follow_actions_state_observed": false
  },
  "content_fingerprint": "b37c538c439e4641dd5ea19ccbb5cbb8d7568e643b8fe4a598ee43e84303c622",
  "structural_boundary_complete": true,
  "content_boundary_complete": false,
  "unavailable_reasons": [
    "MpeExpressionUnavailable: per-note expression fields were not observed",
    "FollowActionsUnavailable: Follow Action settings were not observed"
  ],
  "track_tag": "Sunny|project_a|part_a|track",
  "clip_tag": "Sunny|project_a|part_a|clip",
  "observed_notes_match_request": true,
  "observed_clip_properties_match_request": true
})JSON");
}

json journal(const LomRequest& request, std::string outcome = "acknowledged") {
    const auto& value = std::get<json>(request.args[0]);
    json result{{"document_token", "document_a"},
                {"operation_id", value.at("operation_id")},
                {"name", request.property_or_method},
                {"request", value},
                {"request_fingerprint",
                 *managed_detail::managed_digest(
                     json{{"name", request.property_or_method}, {"request", value}})},
                {"outcome", outcome},
                {"native_mutation_started", outcome != "declined"}};
    if (outcome == "acknowledged")
        result["result"] = observation();
    else
        result["error"] = "Injected retained failure";
    return result;
}

class Peer final : public LomTransport {
  public:
    std::vector<LomRequest> requests;
    std::vector<LomResponse> responses;
    LomResponse send(const LomRequest& request) override {
        requests.push_back(request);
        if (responses.empty())
            return {false, std::nullopt, "No scripted response", LomDeliveryState::NotSent};
        auto result = responses.front();
        responses.erase(responses.begin());
        return result;
    }
    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override { return {}; }
    bool is_connected() const override { return true; }
};

ManagedOperationReceipt prepared() {
    auto request =
        make_managed_clip_request(context, "operation_a", "project_a", "part_a", projection());
    REQUIRE(request);
    auto result = prepare_managed_operation(context, *request);
    REQUIRE(result);
    return *result;
}
} // namespace

TEST_CASE("Managed receipt is persistable before send and uncertain execution only reconciles",
          "[managed][lom]") {
    auto receipt = prepared();
    CHECK(receipt.explicit_retry_safe());
    auto persisted = managed_receipt_from_json(managed_receipt_to_json(receipt));
    REQUIRE(persisted);
    CHECK(persisted->outcome == ManagedOperationOutcome::Prepared);
    Peer peer;
    peer.responses.push_back(
        {false, std::nullopt, "Lost reply", LomDeliveryState::SentWithoutValidResponse});
    auto uncertain = execute_managed_operation(*persisted, peer);
    REQUIRE(uncertain);
    CHECK(uncertain->outcome == ManagedOperationOutcome::Indeterminate);
    CHECK_FALSE(uncertain->explicit_retry_safe());
    CHECK(peer.requests.size() == 1);
    CHECK_FALSE(execute_managed_operation(*uncertain, peer));
    CHECK(peer.requests.size() == 1);
    auto restored = managed_receipt_from_json(managed_receipt_to_json(*uncertain));
    REQUIRE(restored);
    peer.responses.push_back({true, LomValue{journal(receipt.request)}, std::nullopt});
    auto reconciled = reconcile_managed_operation(*restored, peer);
    REQUIRE(reconciled);
    CHECK(reconciled->outcome == ManagedOperationOutcome::Acknowledged);
    CHECK(reconciled->delivery == LomDeliveryState::SentWithoutValidResponse);
    REQUIRE(peer.requests.size() == 2);
    CHECK(peer.requests[1].property_or_method == "sunny_managed_operation");
    CHECK(std::get<json>(peer.requests[1].args[0]) ==
          json{{"document_token", "document_a"}, {"operation_id", "operation_a"}});
    auto binding = managed_binding_receipt(*reconciled);
    REQUIRE(binding);
    CHECK(binding->observation.at("track_index") == 1);
    CHECK_FALSE(make_managed_rebind_request({"bridge_b", "document_b"}, "rebind_a", *binding));
}

TEST_CASE(
    "NotSent permits only explicit exact-token retry and failed reconciliation preserves delivery",
    "[managed][lom]") {
    Peer peer;
    const auto receipt = prepared();
    peer.responses.push_back(
        {false, std::nullopt, "Disconnected before complete send", LomDeliveryState::NotSent});
    auto not_sent = execute_managed_operation(receipt, peer);
    REQUIRE(not_sent);
    CHECK(not_sent->outcome == ManagedOperationOutcome::NotSent);
    CHECK(not_sent->explicit_retry_safe());
    peer.responses.push_back({true, LomValue{journal(receipt.request)}, std::nullopt});
    auto ack = execute_managed_operation(*not_sent, peer);
    REQUIRE(ack);
    CHECK(ack->outcome == ManagedOperationOutcome::Acknowledged);
    CHECK(peer.requests.size() == 2);
    CHECK(LomProtocol::serialize_request(peer.requests[0]) ==
          LomProtocol::serialize_request(peer.requests[1]));
    peer.responses.push_back(
        {false, std::nullopt, "Read-only query disconnected", LomDeliveryState::NotSent});
    auto failed_query = reconcile_managed_operation(*ack, peer);
    REQUIRE(failed_query);
    CHECK(failed_query->outcome == ManagedOperationOutcome::Acknowledged);
    CHECK_FALSE(failed_query->explicit_retry_safe());
    peer.responses.push_back({true, std::nullopt, std::nullopt, LomDeliveryState::NotSent});
    auto malformed_query = reconcile_managed_operation(*ack, peer);
    REQUIRE(malformed_query);
    CHECK(malformed_query->outcome == ManagedOperationOutcome::Acknowledged);
    CHECK(malformed_query->journal == ack->journal);
    CHECK_FALSE(malformed_query->explicit_retry_safe());
}

TEST_CASE("Unknown epoch or operation is never absence proof or safe retry", "[managed][lom]") {
    const auto receipt = prepared();
    Peer peer;
    peer.responses.push_back(
        {false, std::nullopt, "Lost", LomDeliveryState::SentWithoutValidResponse});
    auto uncertain = execute_managed_operation(receipt, peer);
    REQUIRE(uncertain);
    for (const auto& value :
         {json{{"outcome", "unknown_epoch"}, {"document_token", "new_document"}},
          json{{"outcome", "unknown_operation"},
               {"document_token", "document_a"},
               {"operation_id", "operation_a"}}}) {
        peer.responses.push_back({true, LomValue{value}, std::nullopt});
        auto result = reconcile_managed_operation(*uncertain, peer);
        REQUIRE(result);
        CHECK_FALSE(result->explicit_retry_safe());
        CHECK((result->outcome == ManagedOperationOutcome::UnknownEpoch ||
               result->outcome == ManagedOperationOutcome::UnknownOperation));
        CHECK_FALSE(execute_managed_operation(*result, peer));
        REQUIRE(managed_receipt_from_json(managed_receipt_to_json(*result)));
    }
    CHECK(peer.requests.size() == 3);
}

TEST_CASE(
    "Retained partial outcomes and foreign acknowledgements cannot become successful receipts",
    "[managed][lom]") {
    auto receipt = prepared();
    Peer peer;
    peer.responses.push_back(
        {true, LomValue{journal(receipt.request, "indeterminate")}, std::nullopt});
    auto partial = execute_managed_operation(receipt, peer);
    REQUIRE(partial);
    CHECK(partial->outcome == ManagedOperationOutcome::Indeterminate);
    CHECK_FALSE(execute_managed_operation(*partial, peer));
    CHECK_FALSE(managed_binding_receipt(*partial));
    CHECK(peer.requests.size() == 1);
    for (const auto* field : {"operation_id", "document_token", "name", "request_fingerprint"}) {
        auto wrong = journal(receipt.request);
        wrong[field] = "foreign";
        peer.responses.push_back({true, LomValue{wrong}, std::nullopt});
        auto invalid = execute_managed_operation(receipt, peer);
        REQUIRE(invalid);
        CHECK(invalid->outcome == ManagedOperationOutcome::Indeterminate);
        CHECK_FALSE(invalid->explicit_retry_safe());
    }
    auto wrong = journal(receipt.request);
    wrong["request"]["notes"][0]["mute"] = 0;
    peer.responses.push_back({true, LomValue{wrong}, std::nullopt});
    auto invalid = execute_managed_operation(receipt, peer);
    REQUIRE(invalid);
    CHECK(invalid->outcome == ManagedOperationOutcome::Indeterminate);
    CHECK_FALSE(invalid->explicit_retry_safe());
    auto unsafe = managed_receipt_to_json(*partial);
    unsafe["outcome"] = "not_sent";
    unsafe["delivery"] = "not_sent";
    CHECK_FALSE(managed_receipt_from_json(unsafe));
    auto malformed = managed_receipt_to_json(receipt);
    malformed["schema_version"] = true;
    CHECK_FALSE(managed_receipt_from_json(malformed));
    malformed = managed_receipt_to_json(receipt);
    malformed["request"]["bridge_protocol_version"] = 45;
    CHECK_FALSE(managed_receipt_from_json(malformed));
}

TEST_CASE("Incomplete or envelope manifests cannot be rebound or admitted by stale context",
          "[managed][lom]") {
    const auto receipt = prepared();
    Peer peer;
    peer.responses.push_back({true, LomValue{journal(receipt.request)}, std::nullopt});
    auto ack = execute_managed_operation(receipt, peer);
    REQUIRE(ack);
    auto binding = managed_binding_receipt(*ack);
    REQUIRE(binding);
    REQUIRE(managed_binding_from_json(managed_binding_to_json(*binding)));
    const json lane{
        {"parameter", {{"kind", "panning"}}},
        {"clip_end", 4.0},
        {"interpolation", "step"},
        {"points",
         json::array({{{"time", 0.0}, {"value", -0.5}}, {{"time", 2.0}, {"value", 0.25}}})}};
    CHECK(make_managed_envelope_request(context, "envelope_a", *binding, lane));
    CHECK_FALSE(make_managed_envelope_request(
        {"different_bridge", "document_a"}, "envelope_a", *binding, lane));
    binding->observation["structural_boundary_complete"] = false;
    CHECK_FALSE(make_managed_rebind_request(context, "rebind_a", *binding));
    CHECK_FALSE(make_managed_envelope_request(context, "envelope_a", *binding, lane));
    binding->observation["structural_boundary_complete"] = true;
    binding->observation["content_boundary_complete"] = true;
    CHECK_FALSE(make_managed_rebind_request(context, "rebind_a", *binding));
    binding->observation["manifest"]["mpe_note_expression_state_observed"] = true;
    binding->observation["content_fingerprint"] =
        *managed_detail::managed_digest(binding->observation["manifest"]);
    CHECK_FALSE(make_managed_rebind_request(context, "rebind_a", *binding));
    binding->observation["manifest"]["follow_actions_state_observed"] = true;
    binding->observation["content_fingerprint"] =
        *managed_detail::managed_digest(binding->observation["manifest"]);
    CHECK(make_managed_rebind_request(
        context, "rebind_a", *binding)); // hypothetical fully observed peer only
    binding->observation["manifest"]["clip"]["has_envelopes"] = true;
    binding->observation["content_fingerprint"] =
        *managed_detail::managed_digest(binding->observation["manifest"]);
    CHECK_FALSE(make_managed_rebind_request(context, "rebind_a", *binding));
    binding->observation["manifest"]["clip"]["has_envelopes"] = false;
    binding->observation["manifest"]["entire_clip_population_observed"] = false;
    binding->observation["content_fingerprint"] =
        *managed_detail::managed_digest(binding->observation["manifest"]);
    CHECK_FALSE(make_managed_rebind_request(context, "rebind_a", *binding));
}

TEST_CASE("Managed clip projection retains existing beat conversion and explicitly unapplied "
          "global requests",
          "[managed][lom]") {
    CommandBuffer buffer;
    auto record = [&buffer](const LomRequest& request) { REQUIRE(buffer.send(request).success); };
    record(LomProtocol::set_property(LomPaths::song(), "tempo", 120.0));
    record(LomProtocol::call_method(LomPaths::song(), "create_midi_track", {-1}));
    record(LomProtocol::call_method(LomPaths::clip_slot(3, 0), "create_clip", {4.0}));
    record(LomProtocol::set_property(LomPaths::clip(3, 0), "start_marker", 0.0));
    record(LomProtocol::set_property(LomPaths::clip(3, 0), "end_marker", 4.0));
    record(LomProtocol::set_property(LomPaths::clip(3, 0), "signature_numerator", 3));
    record(LomProtocol::set_property(LomPaths::clip(3, 0), "signature_denominator", 8));
    record(LomProtocol::set_property(LomPaths::clip(3, 0), "name", std::string("score part")));
    record(LomProtocol::call_method(LomPaths::clip(3, 0), "sunny_clear_all_envelopes"));
    const auto note = projection().notes[0];
    record(LomProtocol::call_method(
        LomPaths::clip(3, 0), "add_new_notes", {json{{"notes", json::array({note})}}}));
    auto projected = managed_clip_projection(buffer, 3);
    REQUIRE(projected);
    CHECK(projected->clip_end == 4.0);
    CHECK(projected->signature_numerator == 3);
    CHECK(projected->signature_denominator == 8);
    CHECK(projected->notes == projection().notes);
    CHECK(projected->outside_clip_requests.size() == 4);
    CHECK(make_managed_clip_request(context, "create_a", "project_a", "part_a", *projected));
    record(LomProtocol::set_property(LomPaths::clip(3, 0), "muted", true));
    CHECK_FALSE(managed_clip_projection(buffer, 3));
    CHECK_FALSE(managed_clip_projection(buffer, -1));
}

TEST_CASE("Closed managed protocol rejects aliases, extra keys and false coverage declarations",
          "[managed][lom]") {
    const auto valid = prepared().request;
    auto payload = std::get<json>(valid.args[0]);
    CHECK(LomProtocol::validate_request(valid));
    for (const auto* field : {"document_token", "operation_id", "project_key", "binding_key"}) {
        auto invalid = payload;
        invalid[field] = "contains/slash";
        CHECK_FALSE(LomProtocol::validate_request(
            LomProtocol::call_method(LomPaths::song(), "sunny_managed_create_clip", {invalid})));
    }
    auto invalid = payload;
    invalid["foreign_track"] = 0;
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::song(), "sunny_managed_create_clip", {invalid})));
    invalid = payload;
    invalid["notes"][0]["start_time"] = 4.0;
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::song(), "sunny_managed_create_clip", {invalid})));
    invalid = payload;
    invalid["signature_numerator"] = true;
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::song(), "sunny_managed_create_clip", {invalid})));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::track(0), "sunny_managed_create_clip", {payload})));
    CommandBuffer buffer;
    JournaledLomTransport journaled(buffer);
    REQUIRE(journaled.send(LomProtocol::call_method(LomPaths::song(), "sunny_managed_context"))
                .success);
    REQUIRE(journaled
                .send(LomProtocol::call_method(
                    LomPaths::song(),
                    "sunny_managed_operation",
                    {json{{"document_token", "document_a"}, {"operation_id", "operation_a"}}}))
                .success);
    REQUIRE(journaled
                .send(LomProtocol::call_method(LomPaths::song(),
                                               "sunny_managed_observe",
                                               {json{{"document_token", "document_a"},
                                                     {"project_key", "project_a"},
                                                     {"binding_key", "part_a"}}}))
                .success);
    CHECK(journaled.journal().empty());
    CHECK_FALSE(managed_bridge_context(buffer)); // recording is not actual execution
    CHECK_FALSE(execute_managed_operation(prepared(), buffer));
}

TEST_CASE("Managed transport exceptions retain a serializable uncertain receipt",
          "[managed][lom]") {
    class ThrowingPeer final : public LomTransport {
      public:
        int calls = 0;
        LomResponse send(const LomRequest&) override {
            ++calls;
            throw std::runtime_error("Native response failed after delivery may have completed");
        }
        LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override {
            return {};
        }
        bool is_connected() const override { return true; }
    } peer;
    auto receipt = prepared();
    auto result = execute_managed_operation(receipt, peer);
    REQUIRE(result);
    CHECK(result->outcome == ManagedOperationOutcome::Indeterminate);
    CHECK(result->delivery == LomDeliveryState::SentWithoutValidResponse);
    CHECK_FALSE(result->explicit_retry_safe());
    CHECK(peer.calls == 1);
    CHECK(managed_receipt_from_json(managed_receipt_to_json(*result)));
    auto queried = reconcile_managed_operation(*result, peer);
    REQUIRE(queried);
    CHECK(queried->delivery == LomDeliveryState::SentWithoutValidResponse);
    CHECK_FALSE(queried->explicit_retry_safe());
    CHECK(peer.calls == 2);
    CHECK_FALSE(execute_managed_operation(*queried, peer));
    CHECK(peer.calls == 2);
    receipt.delivery = LomDeliveryState::SentWithoutValidResponse;
    CHECK_FALSE(receipt.explicit_retry_safe());
    CHECK_FALSE(execute_managed_operation(receipt, peer));
    CHECK_FALSE(managed_receipt_from_json(managed_receipt_to_json(receipt)));
    CHECK(peer.calls == 2);
}

TEST_CASE("Managed SHA256 matches independent standard vectors and literal Python fingerprints",
          "[managed][lom][fingerprint]") {
    CHECK(managed_detail::sha256("") ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(managed_detail::sha256("abc") ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(managed_detail::sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(managed_detail::sha256(std::string(1000000, 'a')) ==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    const auto receipt = prepared();
    CHECK(managed_detail::managed_digest(
              json{{"name", receipt.request.property_or_method},
                   {"request", std::get<json>(receipt.request.args[0])}}) ==
          "555afd544844f3d89e1c17892c830105ea7ff1e91a45f4ba5b2f64c27eda93f5");
    CHECK(managed_detail::managed_digest(observation().at("manifest")) ==
          "b37c538c439e4641dd5ea19ccbb5cbb8d7568e643b8fe4a598ee43e84303c622");
    CHECK(managed_detail::canonical_managed_bytes(
              json::parse(R"({"a":[null,false,true,1,1.0,-0.0,"é"]})")) ==
          "SM1;O1:{S1:a;A7:[N;F;T;I1;D3ff0000000000000;D8000000000000000;S2:é;]}");
    CHECK(managed_detail::managed_digest(0) != managed_detail::managed_digest(false));
    CHECK(managed_detail::managed_digest(0.0) != managed_detail::managed_digest(-0.0));
    const auto typed = json::parse(
        R"({"bool":[false,true],"float":587555049599699.8,"integer":[-9223372036854775808,18446744073709551615],"null":null,"signed_zero":[-0.0,0.0],"unicode":"é漢𝄞"})");
    // Independently assembled SM1 bytes, hashed with Python hashlib.
    CHECK(managed_detail::managed_digest(typed) ==
          "377adb935f9443dd8ebdb1438b23a229c4c132fa498b775507b40bf21bf2793c");
    CHECK_FALSE(managed_detail::managed_digest(std::numeric_limits<double>::infinity()));
    CHECK_FALSE(managed_detail::managed_digest(std::numeric_limits<double>::quiet_NaN()));
    CHECK_FALSE(managed_detail::managed_digest(std::string("\xc0\x80", 2)));
    CHECK_FALSE(
        managed_detail::managed_digest(std::string(managed_detail::maximum_digest_bytes, 'a')));
    json nested = 0;
    for (unsigned depth = 0; depth < managed_detail::maximum_digest_depth; ++depth)
        nested = json::array({nested});
    CHECK(managed_detail::managed_digest(nested));
    CHECK_FALSE(managed_detail::managed_digest(json::array({nested})));
}

TEST_CASE("Malformed finite observations cannot become acknowledged managed bindings",
          "[managed][lom][observation]") {
    using Mutation = std::pair<const char*, std::function<void(json&)>>;
    const std::vector<Mutation> mutations{
        {"native mutation not started", [](auto& j) { j["native_mutation_started"] = false; }},
        {"foreign project tag",
         [](auto& j) { j["result"]["track_tag"] = "Sunny|foreign_project|part_a|track"; }},
        {"foreign binding tag",
         [](auto& j) { j["result"]["clip_tag"] = "Sunny|project_a|foreign_part|clip"; }},
        {"wrong request hash", [](auto& j) { j["request_fingerprint"] = std::string(64, '0'); }},
        {"Boolean track index", [](auto& j) { j["result"]["track_index"] = true; }},
        {"negative slot index", [](auto& j) { j["result"]["slot_index"] = -1; }},
        {"missing track", [](auto& j) { j["result"]["manifest"].erase("track"); }},
        {"missing clip", [](auto& j) { j["result"]["manifest"].erase("clip"); }},
        {"missing notes", [](auto& j) { j["result"]["manifest"].erase("notes"); }},
        {"missing mixer", [](auto& j) { j["result"]["manifest"].erase("mixer"); }},
        {"missing routing", [](auto& j) { j["result"]["manifest"].erase("routing"); }},
        {"missing counts", [](auto& j) { j["result"]["manifest"].erase("content_counts"); }},
        {"missing coverage",
         [](auto& j) { j["result"]["manifest"].erase("entire_clip_population_observed"); }},
        {"schema Boolean", [](auto& j) { j["result"]["manifest"]["schema_version"] = true; }},
        {"parameter outside domain",
         [](auto& j) { j["result"]["manifest"]["mixer"]["volume"]["value"] = 2.0; }},
        {"Boolean parameter state",
         [](auto& j) { j["result"]["manifest"]["mixer"]["volume"]["state"] = false; }},
        {"wrong note match flag",
         [](auto& j) { j["result"]["manifest"]["notes"] = json::array(); }},
        {"unnormalized integer note field",
         [](auto& j) { j["result"]["manifest"]["notes"][0]["velocity"] = 96; }},
        {"wrong marker match flag",
         [](auto& j) { j["result"]["manifest"]["clip"]["end_marker"] = 8.0; }},
        {"false note match flag",
         [](auto& j) { j["result"]["observed_notes_match_request"] = false; }},
        {"nonboolean marker match",
         [](auto& j) { j["result"]["observed_clip_properties_match_request"] = 1; }},
        {"missing note match", [](auto& j) { j["result"].erase("observed_notes_match_request"); }},
        {"devices contradict structural",
         [](auto& j) { j["result"]["manifest"]["devices_empty"] = false; }},
        {"Session content contradicts structural",
         [](auto& j) { j["result"]["manifest"]["other_session_clips_empty"] = false; }},
        {"unknown takes contradict structural",
         [](auto& j) { j["result"]["manifest"]["content_counts"]["take_lanes"] = nullptr; }},
        {"Arrangement content contradicts structural",
         [](auto& j) { j["result"]["manifest"]["content_counts"]["arrangement_clips"] = 1; }},
        {"Boolean content count",
         [](auto& j) { j["result"]["manifest"]["content_counts"]["take_lanes"] = false; }},
        {"envelope contradicts structural",
         [](auto& j) { j["result"]["manifest"]["clip"]["has_envelopes"] = true; }},
        {"groove contradicts structural",
         [](auto& j) { j["result"]["manifest"]["clip"]["has_groove"] = true; }},
        {"grouped track contradicts structural",
         [](auto& j) { j["result"]["manifest"]["track"]["is_grouped"] = true; }},
        {"frozen track contradicts structural",
         [](auto& j) { j["result"]["manifest"]["track"]["is_frozen"] = true; }},
        {"generated interval contradicts structural",
         [](auto& j) { j["result"]["manifest"]["clip"]["start_marker"] = 1.0; }},
        {"complete outruns unobserved MPE and Follow Actions",
         [](auto& j) { j["result"]["content_boundary_complete"] = true; }},
    };
    const auto receipt = prepared();
    for (const auto& [label, mutate] : mutations) {
        INFO(label);
        auto value = journal(receipt.request);
        mutate(value);
        value["result"]["content_fingerprint"] =
            *managed_detail::managed_digest(value["result"]["manifest"]);
        Peer peer;
        peer.responses.push_back({true, LomValue{value}, std::nullopt});
        const auto rejected = execute_managed_operation(receipt, peer);
        REQUIRE(rejected);
        CHECK(rejected->outcome == ManagedOperationOutcome::Indeterminate);
        CHECK_FALSE(rejected->journal);
        CHECK_FALSE(rejected->explicit_retry_safe());
        CHECK_FALSE(managed_binding_receipt(*rejected));
    }
    auto wrong_digest = journal(receipt.request);
    wrong_digest["result"]["content_fingerprint"] = std::string(64, '0');
    Peer peer;
    peer.responses.push_back({true, LomValue{wrong_digest}, std::nullopt});
    CHECK(execute_managed_operation(receipt, peer)->outcome ==
          ManagedOperationOutcome::Indeterminate);
}

TEST_CASE("Managed observations admit Repeat launch mode and reject undocumented mode four",
          "[managed][lom][observation]") {
    const auto receipt = prepared();
    auto repeat = journal(receipt.request);
    repeat["result"]["manifest"]["clip"]["launch_mode"] = 3;
    // Literal SM1 digests independently recomputed with Python struct/hashlib.
    // The malformed mode has a correct digest, isolating schema admission.
    repeat["result"]["content_fingerprint"] =
        "add07587aa8bf21d1573d7cbab349734b0eda7651a9f8d9672d3ccbab53334c0";
    Peer peer;
    peer.responses.push_back({true, LomValue{repeat}, std::nullopt});
    const auto accepted = execute_managed_operation(receipt, peer);
    REQUIRE(accepted);
    CHECK(accepted->outcome == ManagedOperationOutcome::Acknowledged);
    REQUIRE(managed_binding_receipt(*accepted));

    auto undocumented = repeat;
    undocumented["result"]["manifest"]["clip"]["launch_mode"] = 4;
    undocumented["result"]["content_fingerprint"] =
        "d1e638aa1f2ed469ca98d88b55477d791fd05eabdc04a6211c76fea669d97a5e";
    peer.responses.push_back({true, LomValue{undocumented}, std::nullopt});
    const auto rejected = execute_managed_operation(receipt, peer);
    REQUIRE(rejected);
    CHECK(rejected->outcome == ManagedOperationOutcome::Indeterminate);
    CHECK_FALSE(rejected->journal);
    CHECK_FALSE(rejected->explicit_retry_safe());
    CHECK_FALSE(managed_binding_receipt(*rejected));
}

TEST_CASE("Truthful native value mismatches remain acknowledged evidence and bindings round-trip",
          "[managed][lom][observation]") {
    const auto receipt = prepared();
    auto value = journal(receipt.request);
    value["result"]["manifest"]["notes"] = json::array();
    value["result"]["manifest"]["clip"]["end_marker"] = 8.0;
    value["result"]["manifest"]["track"]["name"] = "User renamed é track";
    value["result"]["observed_notes_match_request"] = false;
    value["result"]["observed_clip_properties_match_request"] = false;
    value["result"]["content_fingerprint"] =
        *managed_detail::managed_digest(value["result"]["manifest"]);
    Peer peer;
    peer.responses.push_back({true, LomValue{value}, std::nullopt});
    const auto acknowledged = execute_managed_operation(receipt, peer);
    REQUIRE(acknowledged);
    REQUIRE(acknowledged->outcome == ManagedOperationOutcome::Acknowledged);
    const auto binding = managed_binding_receipt(*acknowledged);
    REQUIRE(binding);
    REQUIRE(managed_binding_from_json(managed_binding_to_json(*binding)));
    CHECK(binding->observation.at("observed_notes_match_request") == false);
    CHECK(binding->observation.at("observed_clip_properties_match_request") == false);
    CHECK(binding->observation.at("content_boundary_complete") == false);
}

TEST_CASE("Managed native-start acknowledgement distinguishes mutations from read-only rebind",
          "[managed][lom][observation]") {
    const auto create = prepared();
    auto initial = observation();
    ManagedBindingReceipt binding{context, "project_a", "part_a", initial};
    const json lane{
        {"parameter", {{"kind", "panning"}}},
        {"clip_end", 4.0},
        {"interpolation", "step"},
        {"points", {{{"time", 0.0}, {"value", -0.5}}, {{"time", 2.0}, {"value", 0.25}}}}};
    auto replace = make_managed_clip_request(context,
                                             "replace_a",
                                             "project_a",
                                             "part_a",
                                             projection(),
                                             initial.at("content_fingerprint").get<std::string>());
    auto envelope = make_managed_envelope_request(context, "envelope_a", binding, lane);
    REQUIRE(replace);
    REQUIRE(envelope);
    for (const auto& request : {create.request, *replace, *envelope}) {
        auto value = journal(request);
        if (request.property_or_method == "sunny_managed_author_envelope") {
            value["result"]["manifest"]["clip"]["has_envelopes"] = true;
            value["result"]["structural_boundary_complete"] = false;
            value["result"]["content_fingerprint"] =
                *managed_detail::managed_digest(value["result"]["manifest"]);
            value["result"]["acknowledgement"] = {{"action", "created"},
                                                  {"steps_inserted", 2},
                                                  {"parameter",
                                                   {{"matched_name", "Track Panning"},
                                                    {"original_name", "Track Panning"},
                                                    {"minimum", -1.0},
                                                    {"maximum", 1.0},
                                                    {"unit", "internal"},
                                                    {"state", 0},
                                                    {"automation_state", 0}}}};
        }
        const auto receipt = prepare_managed_operation(context, request);
        REQUIRE(receipt);
        Peer peer;
        peer.responses.push_back({true, LomValue{value}, std::nullopt});
        CHECK(execute_managed_operation(*receipt, peer)->outcome ==
              ManagedOperationOutcome::Acknowledged);
        value["native_mutation_started"] = false;
        peer.responses.push_back({true, LomValue{value}, std::nullopt});
        CHECK(execute_managed_operation(*receipt, peer)->outcome ==
              ManagedOperationOutcome::Indeterminate);
    }
    // This is a hypothetical fully covered peer, not a host capability claim.
    auto complete = initial;
    complete["manifest"]["mpe_note_expression_state_observed"] = true;
    complete["manifest"]["follow_actions_state_observed"] = true;
    complete["content_boundary_complete"] = true;
    complete["content_fingerprint"] = *managed_detail::managed_digest(complete["manifest"]);
    binding.observation = complete;
    const auto rebind = make_managed_rebind_request(context, "rebind_a", binding);
    REQUIRE(rebind);
    auto value = journal(*rebind);
    value["result"] = complete;
    value["native_mutation_started"] = false;
    const auto receipt = prepare_managed_operation(context, *rebind);
    REQUIRE(receipt);
    Peer peer;
    peer.responses.push_back({true, LomValue{value}, std::nullopt});
    CHECK(execute_managed_operation(*receipt, peer)->outcome ==
          ManagedOperationOutcome::Acknowledged);
    value["native_mutation_started"] = true;
    peer.responses.push_back({true, LomValue{value}, std::nullopt});
    CHECK(execute_managed_operation(*receipt, peer)->outcome ==
          ManagedOperationOutcome::Indeterminate);
}

TEST_CASE("Finite legacy readback remains limited acknowledged creation evidence",
          "[managed][lom][observation]") {
    const auto receipt = prepared();
    auto value = journal(receipt.request);
    value["result"]["manifest"]["entire_clip_population_observed"] = false;
    value["result"]["manifest"]["content_counts"]["take_lanes"] = nullptr;
    value["result"]["structural_boundary_complete"] = false;
    value["result"]["content_fingerprint"] =
        *managed_detail::managed_digest(value["result"]["manifest"]);
    Peer peer;
    peer.responses.push_back({true, LomValue{value}, std::nullopt});
    const auto result = execute_managed_operation(receipt, peer);
    REQUIRE(result);
    CHECK(result->outcome == ManagedOperationOutcome::Acknowledged);
    const auto binding = managed_binding_receipt(*result);
    REQUIRE(binding);
    CHECK(binding->observation.at("observed_notes_match_request") == true);
    CHECK(binding->observation.at("manifest").at("entire_clip_population_observed") == false);
    CHECK_FALSE(make_managed_rebind_request(context, "rebind_a", *binding));
}
