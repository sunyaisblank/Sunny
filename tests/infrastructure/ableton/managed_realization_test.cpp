#include "managed_note_population_fixture.hpp"
#include "managed_note_update_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <limits>
#include <sunny/core/score/workflows.hpp>
#include <sunny/infrastructure/ableton/deployment.hpp>
#include <sunny/infrastructure/ableton/detail/managed_capacity.hpp>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/managed_realization.hpp>
#include <sunny/infrastructure/formats/ableton_score.hpp>

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

void attach_note_identity(json& actual) {
    auto notes = actual.at("manifest").at("notes");
    int id = 1;
    for (auto& note : notes)
        note["note_id"] = id++;
    actual["note_identity"] = {{"entire_clip_population_observed", true}, {"notes", notes}};
    actual["note_identity_fingerprint"] =
        *managed_detail::managed_digest(actual.at("note_identity"));
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
    attach_note_identity(binding->observation);
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
    CHECK(make_managed_envelope_request(context, "envelope_a", *binding, lane));
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

TEST_CASE("Managed projection consumes actual Score compiler typed note batches",
          "[managed][lom][compiler]") {
    using namespace sunny::core;
    ScoreSpec spec;
    spec.title = "One quarter C4";
    spec.total_bars = 1;
    spec.bpm = 120.0;
    spec.key_root = {0, 0, 4};
    spec.key_accidentals = 0;
    PartDefinition piano;
    piano.name = "Piano";
    piano.instrument_type = InstrumentType::Piano;
    piano.rendering.midi_channel = 1;
    spec.parts.push_back(piano);
    auto score = create_score(spec);
    REQUIRE(score);
    Note note;
    note.pitch = {0, 0, 4};
    note.velocity = {std::nullopt, 80};
    REQUIRE(insert_note(*score, score->parts[0].id, 1, 0, Beat::zero(), note, Beat{1, 4}));

    CommandBuffer recording;
    REQUIRE(formats::compile_to_ableton(*score, recording, 480));
    bool actual_typed_batch = false;
    for (const auto& entry : recording.entries()) {
        if (entry.request.property_or_method != "add_new_notes") continue;
        REQUIRE(entry.request.args.empty());
        REQUIRE(entry.notes.size() == 1);
        actual_typed_batch = true;
    }
    REQUIRE(actual_typed_batch);
    auto projected = managed_clip_projection(recording, 0);
    REQUIRE(projected);
    CHECK(projected->clip_end == 4.0);
    CHECK(projected->signature_numerator == 4);
    CHECK(projected->signature_denominator == 4);
    CHECK(projected->notes == json::array({{{"pitch", 60},
                                            {"start_time", 0.0},
                                            {"duration", 1.0},
                                            {"velocity", 80},
                                            {"mute", false},
                                            {"probability", 1.0},
                                            {"velocity_deviation", 0.0},
                                            {"release_velocity", 64.0}}}));
    CHECK(make_managed_clip_request(context, "create_a", "project_a", "part_a", *projected));
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
    attach_note_identity(initial);
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

namespace {
json note_fixture() {
    return json::parse(managed_note_update_fixture);
}

ManagedBindingReceipt native_note_binding() {
    auto actual = observation();
    actual["note_identity"] =
        note_fixture().at("journal").at("result").at("note_update").at("before_note_identity");
    actual["note_identity_fingerprint"] =
        "f69c3ef4ce93b997267f5f27e8d2bd07545f83568525e4153304da8a1f2bb4c4";
    return {context, "project_a", "part_a", actual};
}

ManagedOperationReceipt note_update_prepared() {
    const auto request = LomProtocol::call_method(
        LomPaths::song(), "sunny_managed_update_notes", {note_fixture().at("request")});
    const auto prepared = prepare_managed_operation(context, request);
    REQUIRE(prepared);
    return *prepared;
}

void refresh_identity(json& observation) {
    observation["note_identity_fingerprint"] =
        *managed_detail::managed_digest(observation.at("note_identity"));
}

void refresh_manifest(json& observation) {
    observation["content_fingerprint"] =
        *managed_detail::managed_digest(observation.at("manifest"));
}
} // namespace

TEST_CASE("Native note update matches literal Python request and independent readback receipt",
          "[managed][lom][notes]") {
    const auto fixture = note_fixture();
    const auto request = make_managed_note_update_request(
        context, "update_a", native_note_binding(), fixture.at("request").at("changes"));
    REQUIRE(request);
    CHECK(std::get<json>(request->args[0]).dump() == fixture.at("request").dump());
    CHECK(LomProtocol::validate_request(*request));
    auto prepared = prepare_managed_operation(context, *request);
    REQUIRE(prepared);
    Peer peer;
    peer.responses.push_back({true, LomValue{fixture.at("journal")}, std::nullopt});
    const auto executed = execute_managed_operation(*prepared, peer);
    REQUIRE(executed);
    CHECK(executed->outcome == ManagedOperationOutcome::Acknowledged);
    CHECK_FALSE(executed->explicit_retry_safe());
    CHECK_FALSE(execute_managed_operation(*executed, peer));
    REQUIRE(executed->journal);
    CHECK(executed->journal->at("request_fingerprint") ==
          "f8b4641ff7afd9f757c5431631c4f58f21d087e8a4c4ee1bc5cc8122a4c70983");
    const auto& evidence = executed->journal->at("result");
    CHECK(evidence.at("content_fingerprint") ==
          "06d4c3ea6f6055b78dc5f2a0a25c428b6cb872d0f97d066244db8c7941b5ae20");
    CHECK(evidence.at("note_identity_fingerprint") ==
          "61b73f065613bed3bb210f16f53e2c3ed6b02dd81f599324ad1d723516ed5c87");
    CHECK(evidence.at("note_identity").at("notes")[0] == json{{"note_id", 1},
                                                              {"pitch", 61},
                                                              {"start_time", 0.25},
                                                              {"duration", 1.5},
                                                              {"velocity", 80.5},
                                                              {"mute", true},
                                                              {"probability", 1.0},
                                                              {"velocity_deviation", 0.0},
                                                              {"release_velocity", 45.25}});
    CHECK(evidence.at("content_boundary_complete") == false);
    const auto binding = managed_binding_receipt(*executed);
    REQUIRE(binding);
    CHECK(managed_binding_from_json(managed_binding_to_json(*binding)));
    const auto restored = managed_receipt_from_json(managed_receipt_to_json(*executed));
    REQUIRE(restored);
    CHECK(restored->journal == executed->journal);
    CHECK(peer.requests.size() == 1);
}

TEST_CASE("Managed read observation retains exact actual context tags and native population",
          "[managed][lom][observation]") {
    const auto raw = note_fixture().at("observation");
    Peer peer;
    peer.responses.push_back({true, LomValue{raw}, std::nullopt});
    JournaledLomTransport guarded(peer, std::nullopt, {}, true);
    const auto observed = observe_managed_binding(context, "project_a", "part_a", guarded);
    REQUIRE(observed);
    CHECK(observed->outcome == ManagedObservationOutcome::Observed);
    CHECK(observed->evidence == raw);
    const auto binding = managed_observed_binding(*observed);
    REQUIRE(binding);
    CHECK(binding->observation == raw.at("observation"));
    CHECK(binding->observation.at("note_identity").at("notes")[0].at("note_id") == 1);
    CHECK(peer.requests.size() == 1);
    CHECK(peer.requests[0].property_or_method == "sunny_managed_observe");
    CHECK(guarded.journal().empty());
    CHECK_FALSE(guarded.plan_diverged());
}

TEST_CASE("Read observations reject foreign logical context and contradictory native ID evidence",
          "[managed][lom][observation]") {
    using Change = std::function<void(json&)>;
    for (const Change& change : std::vector<Change>{
             [](json& raw) { raw["project_key"] = "foreign"; },
             [](json& raw) { raw["binding_key"] = "foreign"; },
             [](json& raw) { raw["context"]["bridge_instance"] = "foreign"; },
             [](json& raw) { raw["context"]["document_token"] = "foreign"; },
             [](json& raw) { raw["ownership_retained"] = false; },
             [](json& raw) { raw["extra"] = 1; },
             [](json& raw) { raw["observation"]["track_tag"] = "Sunny|foreign|part_a|track"; },
             [](json& raw) { raw["observation"].erase("note_identity"); },
             [](json& raw) {
                 raw["observation"]["note_identity_fingerprint"] = std::string(64, '0');
             },
             [](json& raw) {
                 auto& value = raw["observation"];
                 value["note_identity"]["notes"].push_back(value["note_identity"]["notes"][0]);
                 refresh_identity(value);
             },
             [](json& raw) {
                 auto& value = raw["observation"];
                 value["note_identity"]["notes"][0]["pitch"] = 62;
                 refresh_identity(value);
             },
             [](json& raw) {
                 auto& value = raw["observation"];
                 value["note_identity"]["notes"][0]["duration"] = 1;
                 refresh_identity(value);
             },
             [](json& raw) {
                 auto& value = raw["observation"];
                 value["note_identity"]["entire_clip_population_observed"] = false;
                 refresh_identity(value);
             },
             [](json& raw) {
                 auto& value = raw["observation"];
                 value["note_identity"]["notes"][0]["unknown"] = 1;
                 value["note_identity"]["notes"][0].erase("duration");
                 refresh_identity(value);
             }}) {
        auto raw = note_fixture().at("observation");
        change(raw);
        Peer peer;
        peer.responses.push_back({true, LomValue{raw}, std::nullopt});
        CHECK_FALSE(observe_managed_binding(context, "project_a", "part_a", peer));
        CHECK(peer.requests.size() == 1);
    }
}

TEST_CASE(
    "Managed absent-lane candidate permits other envelopes without claiming complete coverage",
    "[managed][lom][envelope]") {
    auto binding = native_note_binding();
    binding.observation["manifest"]["clip"]["has_envelopes"] = true;
    binding.observation["structural_boundary_complete"] = false;
    refresh_manifest(binding.observation);
    REQUIRE(managed_binding_from_json(managed_binding_to_json(binding)));
    const json lane{{"parameter", {{"kind", "volume"}}},
                    {"clip_end", 4.0},
                    {"interpolation", "step"},
                    {"points", {{{"time", 0.0}, {"value", 0.25}}}}};
    REQUIRE(make_managed_envelope_request(context, "volume_a", binding, lane));
    CHECK(binding.observation.at("content_boundary_complete") == false);
    auto wrong_end = lane;
    wrong_end["clip_end"] = 8.0;
    CHECK_FALSE(make_managed_envelope_request(context, "volume_a", binding, wrong_end));
    for (const auto* flag : {"arm", "implicit_arm", "is_frozen", "is_grouped"}) {
        auto bad = binding;
        bad.observation["manifest"]["track"][flag] = true;
        refresh_manifest(bad.observation);
        CHECK_FALSE(make_managed_envelope_request(context, "volume_a", bad, lane));
    }
    auto missing_ids = binding;
    missing_ids.observation.erase("note_identity");
    missing_ids.observation.erase("note_identity_fingerprint");
    CHECK_FALSE(make_managed_envelope_request(context, "volume_a", missing_ids, lane));
}

TEST_CASE("Managed envelope samples retain binding identities and actual independent values",
          "[managed][lom][envelope]") {
    auto raw = note_fixture().at("observation");
    raw["observation"]["manifest"]["clip"]["has_envelopes"] = true;
    raw["observation"]["structural_boundary_complete"] = false;
    refresh_manifest(raw["observation"]);
    raw["envelope"] = {{"has_envelope", true},
                       {"parameter",
                        {{"matched_name", "Track Panning"},
                         {"original_name", "Track Panning"},
                         {"minimum", -1.0},
                         {"maximum", 1.0},
                         {"unit", "internal"},
                         {"state", 0},
                         {"automation_state", 0}}},
                       {"samples",
                        {{{"time", 0.0}, {"value", -0.5}},
                         {{"time", 1.0}, {"value", -0.5}},
                         {{"time", 2.0}, {"value", 0.5}},
                         {{"time", 3.0}, {"value", 0.5}}}}};
    const json selector{{"kind", "panning"}};
    const std::vector<double> times{0.0, 1.0, 2.0, 3.0};
    Peer peer;
    peer.responses.push_back({true, LomValue{raw}, std::nullopt});
    JournaledLomTransport guarded(peer, std::nullopt, {}, true);
    auto sampled =
        sample_managed_envelope(context, "project_a", "part_a", selector, times, guarded);
    REQUIRE(sampled);
    CHECK(sampled->binding.outcome == ManagedObservationOutcome::Observed);
    CHECK(sampled->evidence == raw);
    CHECK(sampled->binding.evidence.contains("envelope") == false);
    REQUIRE(peer.requests.size() == 1);
    CHECK(peer.requests[0].path.to_string() == "song");
    CHECK(peer.requests[0].property_or_method == "sunny_managed_sample_envelope");
    CHECK(guarded.journal().empty());
    CHECK_FALSE(guarded.plan_diverged());

    using Change = std::function<void(json&)>;
    for (const Change& change : std::vector<Change>{
             [](json& value) { value["context"]["bridge_instance"] = "foreign"; },
             [](json& value) { value["project_key"] = "foreign"; },
             [](json& value) { value["binding_key"] = "foreign"; },
             [](json& value) { value["extra"] = 1; },
             [](json& value) { value["envelope"]["samples"].erase(0); },
             [](json& value) { value["envelope"]["samples"][0]["time"] = 0.25; },
             [](json& value) { value["envelope"]["samples"][0]["value"] = 1.25; },
             [](json& value) { value["envelope"]["has_envelope"] = false; },
             [](json& value) { value["envelope"]["parameter"]["original_name"] = "Volume"; },
             [](json& value) { value["envelope"]["parameter"]["maximum"] = 2.0; },
             [](json& value) { value["envelope"]["parameter"]["automation_state"] = 1; },
             [](json& value) { value["envelope"]["parameter"]["state"] = 1; },
             [](json& value) { value["envelope"]["samples"][0]["extra"] = 1; }}) {
        auto bad = raw;
        change(bad);
        peer.responses.push_back({true, LomValue{bad}, std::nullopt});
        CHECK_FALSE(sample_managed_envelope(context, "project_a", "part_a", selector, times, peer));
    }
    for (const std::vector<double>& invalid :
         {std::vector<double>{},
          std::vector<double>{-0.5},
          std::vector<double>{0.0, 0.0},
          std::vector<double>{std::numeric_limits<double>::infinity()}}) {
        const auto calls = peer.requests.size();
        CHECK_FALSE(
            sample_managed_envelope(context, "project_a", "part_a", selector, invalid, peer));
        CHECK(peer.requests.size() == calls);
    }
}

TEST_CASE("Unavailable managed read outcomes never transfer ownership or assert missing mutations",
          "[managed][lom][observation]") {
    for (const auto* name : {"partial_binding", "recovery_unavailable", "unknown_epoch"}) {
        json raw{{"schema_version", 1},
                 {"context", {{"bridge_instance", "bridge_a"}, {"document_token", "document_a"}}},
                 {"project_key", "project_a"},
                 {"binding_key", "part_a"},
                 {"outcome", name},
                 {"ownership_retained", false}};
        auto expected = ManagedObservationOutcome::RecoveryUnavailable;
        if (std::string_view{name} == "partial_binding") {
            raw["native_handles_retained"] = true;
            raw["known_track_index"] = nullptr;
            raw["recovery_available"] = false;
            expected = ManagedObservationOutcome::PartialBinding;
        } else if (std::string_view{name} == "unknown_epoch") {
            raw["context"]["document_token"] = "new_document";
            expected = ManagedObservationOutcome::UnknownEpoch;
        }
        Peer peer;
        peer.responses.push_back({true, LomValue{raw}, std::nullopt});
        const auto observed = observe_managed_binding(context, "project_a", "part_a", peer);
        REQUIRE(observed);
        CHECK(observed->outcome == expected);
        CHECK(observed->evidence == raw);
        CHECK_FALSE(managed_observed_binding(*observed));
    }
}

TEST_CASE("Managed note preflight closes fields domains native expected values and full coverage",
          "[managed][lom][notes]") {
    const auto original = note_fixture().at("request").at("changes");
    using Change = std::function<void(json&)>;
    for (const Change& change : std::vector<Change>{
             [](json& changes) { changes.clear(); },
             [](json& changes) { changes.push_back(changes[0]); },
             [](json& changes) { changes[0]["note_id"] = true; },
             [](json& changes) { changes[0]["note_id"] = std::int64_t{INT32_MAX} + 1; },
             [](json& changes) { changes[0]["extra"] = 1; },
             [](json& changes) { changes[0]["updates"] = json::object(); },
             [](json& changes) { changes[0]["updates"]["probability"] = 0.5; },
             [](json& changes) { changes[0]["updates"]["velocity_deviation"] = 2.0; },
             [](json& changes) { changes[0]["updates"]["pitch"] = 128; },
             [](json& changes) { changes[0]["updates"]["pitch"] = true; },
             [](json& changes) { changes[0]["updates"]["start_time"] = -0.25; },
             [](json& changes) { changes[0]["updates"]["duration"] = 0.0; },
             [](json& changes) {
                 changes[0]["updates"]["duration"] = std::numeric_limits<double>::infinity();
             },
             [](json& changes) { changes[0]["updates"]["velocity"] = 128.0; },
             [](json& changes) { changes[0]["updates"]["release_velocity"] = -1.0; },
             [](json& changes) { changes[0]["updates"]["mute"] = 1; },
             [](json& changes) { changes[0]["expected"].erase("probability"); }}) {
        auto changes = original;
        change(changes);
        CHECK_FALSE(
            make_managed_note_update_request(context, "update_a", native_note_binding(), changes));
        auto request = note_fixture().at("request");
        request["changes"] = changes;
        CHECK_FALSE(LomProtocol::validate_request(
            LomProtocol::call_method(LomPaths::song(), "sunny_managed_update_notes", {request})));
    }
    for (const auto& bad_context : {ManagedBridgeContext{"bridge_b", "document_a"},
                                    ManagedBridgeContext{"bridge_a", "document_b"}})
        CHECK_FALSE(make_managed_note_update_request(
            bad_context, "update_a", native_note_binding(), original));
    auto changes = original;
    changes[0]["expected"]["velocity"] = 81.0;
    CHECK_FALSE(
        make_managed_note_update_request(context, "update_a", native_note_binding(), changes));
    changes = original;
    changes[0]["note_id"] = 3;
    CHECK_FALSE(
        make_managed_note_update_request(context, "update_a", native_note_binding(), changes));
    changes = original;
    changes[0]["updates"]["start_time"] = 4.0;
    CHECK_FALSE(
        make_managed_note_update_request(context, "update_a", native_note_binding(), changes));
    auto legacy = native_note_binding();
    legacy.observation["note_identity"]["entire_clip_population_observed"] = false;
    legacy.observation["manifest"]["entire_clip_population_observed"] = false;
    legacy.observation["structural_boundary_complete"] = false;
    refresh_identity(legacy.observation);
    refresh_manifest(legacy.observation);
    REQUIRE(managed_binding_from_json(managed_binding_to_json(legacy)));
    CHECK_FALSE(make_managed_note_update_request(context, "update_a", legacy, original));
}

TEST_CASE("Managed geometry checks actual half-open collisions without masking positive overlap",
          "[managed][lom][notes]") {
    auto binding = native_note_binding();
    json other{{"note_id", 2},
               {"pitch", 67},
               {"start_time", 2.0},
               {"duration", 0.5},
               {"velocity", 72.0},
               {"mute", true},
               {"probability", 1.0},
               {"velocity_deviation", 0.0},
               {"release_velocity", 32.0}};
    binding.observation["note_identity"]["notes"].push_back(other);
    other.erase("note_id");
    binding.observation["manifest"]["notes"].push_back(other);
    refresh_identity(binding.observation);
    refresh_manifest(binding.observation);
    REQUIRE(managed_binding_from_json(managed_binding_to_json(binding)));
    auto changes = note_fixture().at("request").at("changes");
    for (const double start : {1.5, 2.0, 2.25}) {
        changes[0]["updates"] = {{"pitch", 67}, {"start_time", start}, {"duration", 1.0}};
        CHECK_FALSE(make_managed_note_update_request(context, "update_a", binding, changes));
    }
    for (const double start : {1.0, 2.5}) {
        changes[0]["updates"] = {{"pitch", 67}, {"start_time", start}, {"duration", 1.0}};
        CHECK(make_managed_note_update_request(context, "update_a", binding, changes));
    }
    changes[0]["updates"] = {
        {"pitch", 67}, {"start_time", std::nextafter(1.0, 2.0)}, {"duration", 1.0}};
    CHECK(make_managed_note_update_request(context, "update_a", binding, changes));
    // The first endpoint sum rounds back to exactly2; this next value has a
    // representable positive overlap at the endpoint's double precision.
    changes[0]["updates"]["start_time"] = std::nextafter(2.0, 3.0) - 1.0;
    CHECK_FALSE(make_managed_note_update_request(context, "update_a", binding, changes));
    changes[0]["updates"] = {{"pitch", 62}, {"start_time", 1.0}, {"duration", 1.0}};
    changes.push_back(
        {{"note_id", 2}, {"expected", other}, {"updates", {{"pitch", 62}, {"start_time", 1.5}}}});
    CHECK_FALSE(make_managed_note_update_request(context, "update_a", binding, changes));
    // Native device/envelope populations are untouched by note-only revisions;
    // their unavailability does not imply a destructive complete boundary.
    binding.observation["manifest"]["devices_empty"] = false;
    binding.observation["manifest"]["clip"]["has_envelopes"] = true;
    binding.observation["structural_boundary_complete"] = false;
    refresh_manifest(binding.observation);
    changes = note_fixture().at("request").at("changes");
    CHECK(make_managed_note_update_request(context, "update_a", binding, changes));
    CHECK_FALSE(make_managed_rebind_request(context, "rebind_a", binding));
}

TEST_CASE("Managed note receipts recompute actual update and preservation flags rather than "
          "echoing intent",
          "[managed][lom][notes]") {
    const auto prepared = note_update_prepared();
    auto truthful = note_fixture().at("journal");
    // A successful native apply with unchanged actual values is retained as
    // acknowledged evidence; requested-value verification is independently false.
    truthful["result"]["manifest"] = observation().at("manifest");
    truthful["result"]["note_identity"] =
        truthful.at("result").at("note_update").at("before_note_identity");
    truthful["result"]["note_update"]["observed_updates_match_request"] = false;
    refresh_identity(truthful["result"]);
    refresh_manifest(truthful["result"]);
    Peer peer;
    peer.responses.push_back({true, LomValue{truthful}, std::nullopt});
    const auto retained = execute_managed_operation(prepared, peer);
    REQUIRE(retained);
    CHECK(retained->outcome == ManagedOperationOutcome::Acknowledged);
    CHECK(managed_binding_receipt(*retained));
    CHECK_FALSE(retained->explicit_retry_safe());
    using Change = std::function<void(json&)>;
    for (const Change& change : std::vector<Change>{
             [](json& value) { value["native_mutation_started"] = false; },
             [](json& value) {
                 value["result"]["note_update"]["observed_updates_match_request"] = true;
             },
             [](json& value) {
                 value["result"]["note_update"]["untouched_notes_preserved"] = false;
             },
             [](json& value) { value["result"]["note_update"]["note_ids_preserved"] = false; },
             [](json& value) { value["result"]["note_update"]["notes_submitted"] = 2; },
             [](json& value) {
                 value["result"]["note_update"]["before_note_identity_fingerprint"] =
                     std::string(64, '0');
             },
             [](json& value) {
                 value["result"]["note_update"]["before_manifest"]["clip"]["end_marker"] = 8.0;
             },
             [](json& value) { value["result"]["track_tag"] = "Sunny|foreign|part_a|track"; },
             [](json& value) { value["result"]["note_update"]["extra"] = 1; }}) {
        auto value = truthful;
        change(value);
        peer.responses.push_back({true, LomValue{value}, std::nullopt});
        const auto rejected = execute_managed_operation(prepared, peer);
        REQUIRE(rejected);
        CHECK(rejected->outcome == ManagedOperationOutcome::Indeterminate);
        CHECK_FALSE(rejected->explicit_retry_safe());
    }
    // A transport uncertainty queries the same original token exactly once.
    peer.responses.push_back(
        {false, std::nullopt, "Lost native reply", LomDeliveryState::SentWithoutValidResponse});
    const auto uncertain = execute_managed_operation(prepared, peer);
    REQUIRE(uncertain);
    CHECK(uncertain->outcome == ManagedOperationOutcome::Indeterminate);
    peer.responses.push_back({true, LomValue{truthful}, std::nullopt});
    const auto reconciled = reconcile_managed_operation(*uncertain, peer);
    REQUIRE(reconciled);
    CHECK(reconciled->outcome == ManagedOperationOutcome::Acknowledged);
    CHECK(peer.requests.back().property_or_method == "sunny_managed_operation");
}

TEST_CASE("Untouched finite note fields cannot establish a finite geometry endpoint",
          "[managed][lom][notes]") {
    auto binding = native_note_binding();
    json other{{"note_id", 2},
               {"pitch", 67},
               {"start_time", 1e308},
               {"duration", 1e308},
               {"velocity", 72.0},
               {"mute", false},
               {"probability", 1.0},
               {"velocity_deviation", 0.0},
               {"release_velocity", 32.0}};
    binding.observation["note_identity"]["notes"].push_back(other);
    other.erase("note_id");
    binding.observation["manifest"]["notes"].push_back(other);
    refresh_identity(binding.observation);
    refresh_manifest(binding.observation);
    // Finite evidence is retainable. This deliberately does not prove that a
    // real Live host admits an enormous range outside the generated markers.
    REQUIRE(managed_binding_from_json(managed_binding_to_json(binding)));
    auto changes = note_fixture().at("request").at("changes");
    CHECK_FALSE(make_managed_note_update_request(context, "update_a", binding, changes));
    changes[0]["updates"] = {{"velocity", 81.0}};
    CHECK(make_managed_note_update_request(context, "values_a", binding, changes));
}

TEST_CASE("Managed same-pitch start swaps cannot rely on unqualified native batch atomicity",
          "[managed][lom][notes]") {
    auto binding = native_note_binding();
    auto other = binding.observation["note_identity"]["notes"][0];
    other["note_id"] = 2;
    other["start_time"] = 2.0;
    binding.observation["note_identity"]["notes"].push_back(other);
    other.erase("note_id");
    binding.observation["manifest"]["notes"].push_back(other);
    refresh_identity(binding.observation);
    refresh_manifest(binding.observation);
    REQUIRE(managed_binding_from_json(managed_binding_to_json(binding)));
    json changes = json::array();
    for (auto actual : binding.observation["note_identity"]["notes"]) {
        const int id = actual.at("note_id");
        actual.erase("note_id");
        changes.push_back({{"note_id", id},
                           {"expected", actual},
                           {"updates", {{"start_time", id == 1 ? 2.0 : 0.0}}}});
    }
    // Final [0,1)/[2,3) remain disjoint; each destination covers the other
    // retained baseline interval. This is an admission boundary, not a host result.
    CHECK_FALSE(make_managed_note_update_request(context, "swap", binding, changes));
    changes[0]["updates"] = {{"pitch", 62}};
    changes[1]["updates"] = {{"pitch", 62}};
    CHECK(make_managed_note_update_request(context, "transpose", binding, changes));
    // Adjacent same-pitch destinations remain safe against old/new intervals.
    changes[0]["updates"] = {{"start_time", 1.0}};
    changes[1]["updates"] = {{"start_time", 3.0}};
    CHECK(make_managed_note_update_request(context, "adjacent", binding, changes));
}

namespace {
json population_fixture() {
    return json::parse(managed_note_population_fixture);
}
ManagedBindingReceipt population_before_binding() {
    const auto fixture = population_fixture();
    auto before = fixture.at("journal").at("result");
    const auto& supplement = before.at("note_population_update");
    const auto manifest = supplement.at("before_manifest");
    const auto identity = supplement.at("before_note_identity");
    before.erase("note_population_update");
    before["manifest"] = manifest;
    before["note_identity"] = identity;
    refresh_manifest(before);
    refresh_identity(before);
    return {context, "project_a", "part_a", before};
}
ManagedOperationReceipt population_prepared() {
    const auto request = LomProtocol::call_method(LomPaths::song(),
                                                  "sunny_managed_revise_note_population",
                                                  {population_fixture().at("request")});
    auto prepared = prepare_managed_operation(context, request);
    REQUIRE(prepared);
    return *prepared;
}
} // namespace

TEST_CASE(
    "Population receipt closes literal independent Python IDs geometry and insertion association",
    "[managed][lom][population]") {
    const auto fixture = population_fixture();
    CHECK(fixture.at("journal").at("request_fingerprint") ==
          "72e7310be47b01eefce9c56083500d14e133cbfec5990a766195809b81da712e");
    const auto binding = population_before_binding();
    const auto& intent = fixture.at("request");
    const auto request = make_managed_note_population_request(context,
                                                              "population_a",
                                                              binding,
                                                              intent.at("changes"),
                                                              intent.at("deletions"),
                                                              intent.at("additions"));
    REQUIRE(request);
    CHECK(json::parse(LomProtocol::serialize_request(*request)).at("args").at(0) == intent);
    const auto prepared = prepare_managed_operation(context, *request);
    REQUIRE(prepared);
    Peer peer;
    peer.responses.push_back({true, LomValue{fixture.at("journal")}, std::nullopt});
    auto executed = execute_managed_operation(*prepared, peer);
    REQUIRE(executed);
    CHECK(executed->outcome == ManagedOperationOutcome::Acknowledged);
    CHECK_FALSE(executed->explicit_retry_safe());
    const auto retained = managed_binding_receipt(*executed);
    REQUIRE(retained);
    CHECK(managed_binding_from_json(managed_binding_to_json(*retained)));
    const auto& notes = retained->observation.at("note_identity").at("notes");
    REQUIRE(notes.size() == 2);
    CHECK(notes.at(0).at("note_id") == 1);
    CHECK(notes.at(0).at("pitch") == 62);
    CHECK(notes.at(0).at("start_time") == 0.0);
    CHECK(notes.at(0).at("duration") == 1.0);
    CHECK(notes.at(0).at("velocity") == 96.0);
    CHECK(notes.at(1).at("note_id") == 3);
    CHECK(notes.at(1).at("pitch") == 64);
    CHECK(notes.at(1).at("start_time") == 2.0);
    CHECK(retained->observation.at("note_population_update").at("addition_associations") ==
          json::array({{{"note_key", "e3_n0"}, {"note_id", 3}}}));
    const auto calls = peer.requests.size();
    CHECK_FALSE(execute_managed_operation(*executed, peer));
    CHECK(peer.requests.size() == calls);
}

TEST_CASE(
    "Population contradictory closure is uncertain but truthful final mismatch remains evidence",
    "[managed][lom][population]") {
    const auto prepared = population_prepared();
    using Change = std::function<void(json&)>;
    for (const Change& change : std::vector<Change>{
             [](json& j) { j["native_mutation_started"] = false; },
             [](json& j) {
                 j["result"]["note_population_update"]["observed_changes_match_request"] = false;
             },
             [](json& j) {
                 j["result"]["note_population_update"]["observed_deletions_absent"] = false;
             },
             [](json& j) {
                 j["result"]["note_population_update"]["observed_additions_match_request"] = false;
             },
             [](json& j) {
                 j["result"]["note_population_update"]["retained_note_ids_preserved"] = false;
             },
             [](json& j) {
                 j["result"]["note_population_update"]["observed_population_cardinality_match"] =
                     false;
             },
             [](json& j) {
                 j["result"]["note_population_update"]["addition_associations"][0]["note_id"] = 1;
             },
             [](json& j) { j["progress"]["returned_calls"] = json::array(); },
             [](json& j) { j["progress"]["returned_added_note_ids"] = json::array({999}); },
             [](json& j) {
                 j["result"]["note_population_update"]["returned_added_note_ids"] =
                     json::array({2});
             },
             [](json& j) {
                 j["result"]["note_population_update"]["before_note_identity_fingerprint"] =
                     std::string(64, '0');
             },
             [](json& j) { j["result"]["note_population_update"]["extra"] = 0; }}) {
        auto bad = population_fixture().at("journal");
        change(bad);
        Peer peer;
        peer.responses.push_back({true, LomValue{bad}, std::nullopt});
        const auto observed = execute_managed_operation(prepared, peer);
        REQUIRE(observed);
        CHECK(observed->outcome == ManagedOperationOutcome::Indeterminate);
        CHECK_FALSE(managed_binding_receipt(*observed));
        CHECK_FALSE(observed->explicit_retry_safe());
    }
    auto truthful = population_fixture().at("journal");
    auto& observation = truthful["result"];
    observation["note_identity"]["notes"][1]["velocity"] = 87.0;
    observation["manifest"]["notes"][1]["velocity"] = 87.0;
    observation["note_population_update"]["observed_additions_match_request"] = false;
    observation["note_population_update"]["addition_associations"] = json::array();
    refresh_manifest(observation);
    refresh_identity(observation);
    Peer peer;
    peer.responses.push_back({true, LomValue{truthful}, std::nullopt});
    const auto retained = execute_managed_operation(prepared, peer);
    REQUIRE(retained);
    CHECK(retained->outcome == ManagedOperationOutcome::Acknowledged);
    CHECK(managed_binding_receipt(*retained));
}

TEST_CASE("Population preflight admits explicit deletion space and rejects malformed or colliding "
          "revisions",
          "[managed][lom][population]") {
    const auto original = population_fixture().at("request");
    auto binding = population_before_binding();
    using Change = std::function<void(json&)>;
    for (const Change& change : std::vector<Change>{
             [](json& r) { r["deletions"][0]["note_id"] = 999; },
             [](json& r) {
                 r["deletions"][0] = {{"note_id", 1}, {"expected", r["changes"][0]["expected"]}};
             },
             [](json& r) {
                 r["additions"][0]["note"]["pitch"] = 62;
                 r["additions"][0]["note"]["start_time"] = 0.5;
             },
             [](json& r) { r["additions"].push_back(r["additions"][0]); },
             [](json& r) { r["additions"][0]["note_key"] = "e0_n0"; },
             [](json& r) { r["additions"][0]["note_key"] = "e18446744073709551616_n0"; },
             [](json& r) { r["additions"][0]["note_key"] = "e3_n65536"; },
             [](json& r) {
                 r["additions"][0]["note"]["duration"] = std::numeric_limits<double>::infinity();
             },
             [](json& r) { r["changes"] = r["deletions"] = r["additions"] = json::array(); }}) {
        auto invalid = original;
        change(invalid);
        CHECK_FALSE(make_managed_note_population_request(context,
                                                         "population_a",
                                                         binding,
                                                         invalid.at("changes"),
                                                         invalid.at("deletions"),
                                                         invalid.at("additions")));
    }
    auto useful = original;
    useful["additions"][0]["note"]["pitch"] = 67; // freed G4 ID2 interval
    CHECK(make_managed_note_population_request(context,
                                               "population_a",
                                               binding,
                                               useful.at("changes"),
                                               useful.at("deletions"),
                                               useful.at("additions")));
    CHECK(make_managed_note_population_request(
        context, "population_a", binding, json::array(), useful.at("deletions"), json::array()));
    CHECK_FALSE(
        make_managed_note_population_request(context,
                                             "population_a",
                                             binding,
                                             json::array(),
                                             json::array(),
                                             useful.at("additions"))); // same G4 interval retained
}

TEST_CASE("Complete reply byte admission rejects 26000 native notes without a count-only cap",
          "[managed][lom][notes][capacity]") {
    auto binding = native_note_binding();
    auto& observation = binding.observation;
    observation["note_identity"]["notes"] = json::array();
    observation["manifest"]["notes"] = json::array();
    observation["manifest"]["clip"]["end_marker"] = 100000.0;
    observation["manifest"]["clip"]["loop_end"] = 100000.0;
    for (std::int32_t id = 1; id <= 26000; ++id) {
        json values{{"pitch", 60},
                    {"start_time", id * 2.0 + 0.123456789012345},
                    {"duration", 0.123456789012345},
                    {"velocity", 87.12345678901234},
                    {"mute", false},
                    {"probability", 0.987654321012345},
                    {"velocity_deviation", -12.12345678901234},
                    {"release_velocity", 77.12345678901234}};
        observation["manifest"]["notes"].push_back(values);
        values["note_id"] = id;
        observation["note_identity"]["notes"].push_back(values);
    }
    refresh_manifest(observation);
    refresh_identity(observation);
    auto expected = observation.at("note_identity").at("notes").at(0);
    expected.erase("note_id");
    json changes =
        json::array({{{"note_id", 1}, {"expected", expected}, {"updates", {{"velocity", 96.0}}}}});
    const auto rejected = make_managed_note_update_request(context, "update_a", binding, changes);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == sunny::core::ErrorCode::ManagedReplyCapacityExceeded);
    CHECK(observation.dump().size() < 16U * 1024U * 1024U);
    json scalar{{"unicode", "é漢𝄞\x7f\n"}, {"float", std::numeric_limits<double>::max()}};
    CHECK(*managed_detail::json_wire_bound(scalar) >= scalar.dump(-1, ' ', true).size());
    CHECK(managed_detail::note_array_bound(26000, true) >
          managed_detail::note_array_bound(26000, false));
}

TEST_CASE("Creation admits known reply bytes before a durable native dispatch fence",
          "[managed][lom][capacity]") {
    auto small = projection();
    REQUIRE(make_managed_clip_request(context, "create_a", "project_a", "part_a", small));
    auto large = projection();
    large.clip_end = 100000.0;
    large.notes = json::array();
    for (std::int32_t i = 1; i <= 26000; ++i)
        large.notes.push_back(json{{"pitch", 60},
                                   {"start_time", i * 2.0 + 0.123456789012345},
                                   {"duration", 0.123456789012345},
                                   {"velocity", 87},
                                   {"mute", false},
                                   {"probability", 1.0},
                                   {"velocity_deviation", 0.0},
                                   {"release_velocity", 77.0}});
    const auto rejected =
        make_managed_clip_request(context, "create_a", "project_a", "part_a", large);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == sunny::core::ErrorCode::ManagedReplyCapacityExceeded);
    CHECK(large.notes.dump().size() < 16U * 1024U * 1024U);

    // Projection remains valid musical evidence. Reply admission belongs to the
    // native builder, allowing the owning facade to report capacity pre-fence.
    CommandBuffer recording;
    REQUIRE(recording
                .send(LomProtocol::call_method(
                    LomPaths::clip_slot(0, 0), "create_clip", {large.clip_end}))
                .success);
    REQUIRE(recording.send(LomProtocol::set_property(LomPaths::clip(0, 0), "start_marker", 0.0))
                .success);
    REQUIRE(recording
                .send(LomProtocol::set_property(LomPaths::clip(0, 0), "end_marker", large.clip_end))
                .success);
    REQUIRE(
        recording.send(LomProtocol::set_property(LomPaths::clip(0, 0), "signature_numerator", 4))
            .success);
    REQUIRE(
        recording.send(LomProtocol::set_property(LomPaths::clip(0, 0), "signature_denominator", 4))
            .success);
    REQUIRE(recording
                .send(LomProtocol::call_method(
                    LomPaths::clip(0, 0), "add_new_notes", {json{{"notes", large.notes}}}))
                .success);
    const auto projected = managed_clip_projection(recording, 0);
    REQUIRE(projected);
    CHECK(projected->notes.size() == 26000);
    const auto native =
        make_managed_clip_request(context, "create_a", "project_a", "part_a", *projected);
    REQUIRE_FALSE(native);
    CHECK(native.error() == sunny::core::ErrorCode::ManagedReplyCapacityExceeded);
}

TEST_CASE("Initial managed lanes reject native call and complete response "
          "excess before preparation",
          "[managed][envelope][capacity]") {
    auto actual = observation();
    attach_note_identity(actual);
    ManagedBindingReceipt binding{context, "project_a", "part_a", actual};
    json points = json::array();
    for (int i = 0; i < 64; ++i)
        points.push_back({{"time", i / 32.0}, {"value", 0.25}});
    json lane{{"parameter", {{"kind", "panning"}}},
              {"clip_end", 4.0},
              {"interpolation", "step"},
              {"points", points}};
    REQUIRE(make_managed_envelope_request(context, "lane_64", binding, lane));
    lane["points"].push_back({{"time", 2.0}, {"value", 0.25}});
    const auto oversized_calls = make_managed_envelope_request(context, "lane_65", binding, lane);
    REQUIRE_FALSE(oversized_calls);
    CHECK(oversized_calls.error() == sunny::core::ErrorCode::ProtocolError);
    // Actual finite observation arrays, not a tiny request-count substitute.
    const auto note = actual.at("manifest").at("notes").at(0);
    actual["manifest"]["notes"] = json::array();
    actual["note_identity"]["notes"] = json::array();
    for (int id = 1; id <= 26000; ++id) {
        actual["manifest"]["notes"].push_back(note);
        auto identified = note;
        identified["note_id"] = id;
        actual["note_identity"]["notes"].push_back(std::move(identified));
    }
    actual["content_fingerprint"] = *managed_detail::managed_digest(actual["manifest"]);
    actual["note_identity_fingerprint"] = *managed_detail::managed_digest(actual["note_identity"]);
    binding.observation = std::move(actual);
    lane["points"] = {{{"time", 0.0}, {"value", 0.25}}};
    const auto oversized_reply =
        make_managed_envelope_request(context, "lane_reply", binding, lane);
    REQUIRE_FALSE(oversized_reply);
    CHECK(oversized_reply.error() == sunny::core::ErrorCode::ManagedReplyCapacityExceeded);
}
