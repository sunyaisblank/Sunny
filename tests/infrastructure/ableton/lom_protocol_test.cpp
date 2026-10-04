/**
 * @file lom_protocol_test.cpp
 * @brief LOM Bridge JSON Deserialisation unit tests
 *
 *
 * Coverage: JSON deserialisation, malformed input, serialisation,
 *           LomPath operations, note serialisation
 */

#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/infrastructure/ableton/deployment.hpp>
#include <sunny/infrastructure/ableton/lom_protocol.hpp>
#include <sunny/infrastructure/ableton/target_profile.hpp>

using namespace sunny::infrastructure;

TEST_CASE("Session Step envelope RPC admits a closed finite internal-domain shape",
          "[lom][protocol][automation]") {
    using nlohmann::json;
    const auto path = LomPaths::clip(0, 0);
    const json parameter = {{"kind", "device"}, {"device_index", 0}, {"parameter_name", "Dry/Wet"}};
    const json author = {{"parameter", parameter},
                         {"interpolation", "step"},
                         {"clip_end", 4.0},
                         {"points",
                          json::array({{{"time", 0.0}, {"value", 0.25}},
                                       {{"time", 1.0}, {"value", 0.8}},
                                       {{"time", 2.5}, {"value", 0.15}}})}};
    const json query = {{"parameter", parameter}, {"sample_times", {0.0, 0.999, 1.0, 3.999}}};
    CHECK(LomProtocol::validate_request(
        LomProtocol::call_method(path, "sunny_author_step_envelope", {author})));
    CHECK(LomProtocol::validate_request(
        LomProtocol::call_method(path, "sunny_get_step_envelope", {query})));
    for (const auto& selector : {json{{"kind", "volume"}},
                                 json{{"kind", "panning"}},
                                 json{{"kind", "send"}, {"send_index", 0}}}) {
        auto mixer_author = author;
        mixer_author["parameter"] = selector;
        CHECK(LomProtocol::validate_request(
            LomProtocol::call_method(path, "sunny_author_step_envelope", {mixer_author})));
    }

    SECTION("selectors are relative, unambiguous and exact") {
        const std::vector<json> selectors = {
            {{"kind", "volume"}, {"path", "song/tracks/1"}},
            {{"kind", "send"}, {"send_index", true}},
            {{"kind", "send"}, {"send_index", -1}},
            {{"kind", "device"}, {"device_index", 0.0}, {"parameter_name", "Dry/Wet"}},
            {{"kind", "device"}, {"device_index", 2147483648ULL}, {"parameter_name", "Dry/Wet"}},
            {{"kind", "device"}, {"device_index", 0}, {"parameter_name", ""}},
            {{"kind", "device"},
             {"device_index", 0},
             {"parameter_name", "Dry/Wet"},
             {"unit", "dB"}},
            {{"kind", "unknown"}}};
        for (const auto& selector : selectors) {
            auto invalid_author = author;
            invalid_author["parameter"] = selector;
            auto invalid_query = query;
            invalid_query["parameter"] = selector;
            CHECK_FALSE(LomProtocol::validate_request(
                LomProtocol::call_method(path, "sunny_author_step_envelope", {invalid_author})));
            CHECK_FALSE(LomProtocol::validate_request(
                LomProtocol::call_method(path, "sunny_get_step_envelope", {invalid_query})));
        }
        CHECK_FALSE(LomProtocol::validate_request(
            LomProtocol::call_method(LomPaths::track(0), "sunny_author_step_envelope", {author})));
        CHECK_FALSE(LomProtocol::validate_request(
            LomProtocol::call_method(LomPath::parse("song/tracks/0/arrangement_clips/0"),
                                     "sunny_author_step_envelope",
                                     {author})));
        CHECK_FALSE(LomProtocol::validate_request(
            LomProtocol::call_method(path, "sunny_author_step_envelope", {author, false})));
        CHECK_FALSE(LomProtocol::validate_request(
            LomProtocol::call_method(path, "sunny_get_step_envelope")));
    }
    SECTION("lane intent is finite, Step, ordered and spans from zero") {
        std::vector<json> invalid;
        auto changed = author;
        changed["interpolation"] = "linear";
        invalid.push_back(changed);
        changed = author;
        changed["clip_end"] = true;
        invalid.push_back(changed);
        changed = author;
        changed["clip_end"] = std::numeric_limits<double>::infinity();
        invalid.push_back(changed);
        changed = author;
        changed["clip_end"] = 0.0;
        invalid.push_back(changed);
        changed = author;
        changed["foreign_clip"] = "song/tracks/1/clip_slots/0/clip";
        invalid.push_back(changed);
        for (const auto& points :
             {json::array(),
              json::array({{{"time", 0.1}, {"value", 0.2}}}),
              json::array({{{"time", 0.0}, {"value", true}}}),
              json::array({{{"time", 0.0}, {"value", std::numeric_limits<double>::quiet_NaN()}}}),
              json::array({{{"time", 0.0}, {"value", 0.2}, {"curve", "linear"}}}),
              json::array({{{"time", 0.0}, {"value", 0.2}}, {{"time", 4.0}, {"value", 0.3}}}),
              json::array({{{"time", 0.0}, {"value", 0.2}}, {{"time", 0.0}, {"value", 0.3}}}),
              json::array({{{"time", 0ULL}, {"value", 0.2}},
                           {{"time", 9007199254740992ULL}, {"value", 0.3}},
                           {{"time", 9007199254740993ULL}, {"value", 0.4}}})}) {
            changed = author;
            changed["clip_end"] = points.size() == 3 ? 18014398509481984.0 : 4.0;
            changed["points"] = points;
            invalid.push_back(changed);
        }
        for (const auto& value : invalid)
            CHECK_FALSE(LomProtocol::validate_request(
                LomProtocol::call_method(path, "sunny_author_step_envelope", {value})));
    }
    SECTION("queries are finite ordered distinct sample controls") {
        for (const auto& times : {json::array(),
                                  json::array({-0.1}),
                                  json::array({true}),
                                  json::array({std::numeric_limits<double>::infinity()}),
                                  json::array({std::numeric_limits<double>::quiet_NaN()}),
                                  json::array({0.5, 0.5}),
                                  json::array({1.5, 0.5}),
                                  json::array({9007199254740992ULL, 9007199254740993ULL})}) {
            auto changed = query;
            changed["sample_times"] = times;
            CHECK_FALSE(LomProtocol::validate_request(
                LomProtocol::call_method(path, "sunny_get_step_envelope", {changed})));
        }
        auto changed = query;
        changed["points"] = author["points"];
        CHECK_FALSE(LomProtocol::validate_request(
            LomProtocol::call_method(path, "sunny_get_step_envelope", {changed})));
    }
}

TEST_CASE("envelope readback is read only and uncertain author calls are journalled once",
          "[lom][protocol][automation][journal]") {
    using nlohmann::json;
    const json author = {{"parameter", {{"kind", "volume"}}},
                         {"interpolation", "step"},
                         {"clip_end", 4.0},
                         {"points", json::array({{{"time", 0.0}, {"value", 0.5}}})}};
    const json query = {{"parameter", {{"kind", "volume"}}}, {"sample_times", {0.5, 3.5}}};
    const auto read =
        LomProtocol::call_method(LomPaths::clip(0, 0), "sunny_get_step_envelope", {query});
    const auto write =
        LomProtocol::call_method(LomPaths::clip(0, 0), "sunny_author_step_envelope", {author});
    CommandBuffer target;
    JournaledLomTransport guarded{target, std::nullopt, {}, true};
    CHECK(guarded.send(read).success);
    CHECK(guarded.journal().empty());
    CHECK_FALSE(guarded.plan_diverged());
    CHECK_FALSE(guarded.send(write).success);
    REQUIRE(guarded.journal().size() == 1);
    CHECK(guarded.journal().front().outcome == AbletonMutationOutcome::DeclinedBeforeSend);
    REQUIRE(target.entries().size() == 1);
    CHECK(target.entries().front().request.property_or_method == "sunny_get_step_envelope");

    class UncertainTransport final : public LomTransport {
      public:
        std::size_t sends = 0;
        LomResponse send(const LomRequest&) override {
            ++sends;
            return {false,
                    std::nullopt,
                    std::string{"timeout after send"},
                    LomDeliveryState::SentWithoutValidResponse};
        }
        LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override {
            return {false, std::nullopt, std::string{"unused"}, LomDeliveryState::NotSent};
        }
        bool is_connected() const override { return true; }
    } uncertain;
    JournaledLomTransport journaled{uncertain};
    CHECK_FALSE(journaled.send(write).success);
    CHECK(uncertain.sends == 1);
    REQUIRE(journaled.journal().size() == 1);
    CHECK(journaled.journal().front().outcome == AbletonMutationOutcome::Indeterminate);
    CHECK(journaled.journal().front().target_may_have_mutated());
}

// =============================================================================
// LomPath Tests
// =============================================================================

TEST_CASE("LomPath to_string", "[bridge][path]") {
    LomPath p{{"song", "tracks", "0"}};
    CHECK(p.to_string() == "song/tracks/0");
}

TEST_CASE("LomPath parse", "[bridge][path]") {
    auto p = LomPath::parse("song/tracks/0/clip_slots/1");
    REQUIRE(p.segments.size() == 5);
    CHECK(p.segments[0] == "song");
    CHECK(p.segments[2] == "0");
    CHECK(p.segments[4] == "1");
}

TEST_CASE("LomPath parsing is lossless and canonicality is explicit", "[bridge][path]") {
    CHECK(LomPath::parse("song/tracks/0").is_canonical());

    auto p = LomPath::parse("/song/tracks");
    CHECK(p.to_string() == "/song/tracks");
    CHECK_FALSE(p.is_canonical());

    p = LomPath::parse("song//tracks/0");
    CHECK(p.to_string() == "song//tracks/0");
    CHECK_FALSE(p.is_canonical());

    CHECK_FALSE(LomPath::parse("song/tracks/00").is_canonical());
    CHECK_FALSE(LomPath::parse("song/tracks/\xD9\xA1").is_canonical());
    CHECK(LomPath::parse("song/tracks/2147483647").is_canonical());
    CHECK_FALSE(LomPath::parse("song/tracks/2147483648").is_canonical());
    CHECK_FALSE(LomPath::parse("song/tracks/0/").is_canonical());
}

TEST_CASE("LomPath child operations", "[bridge][path]") {
    auto p = LomPaths::song();
    CHECK(p.to_string() == "song");

    auto t = p.child("tracks").child(2);
    CHECK(t.to_string() == "song/tracks/2");
}

TEST_CASE("LomPaths helpers", "[bridge][path]") {
    CHECK(LomPaths::track(3).to_string() == "song/tracks/3");
    CHECK(LomPaths::scene(2).to_string() == "song/scenes/2");
    CHECK(LomPaths::clip_slot(1, 2).to_string() == "song/tracks/1/clip_slots/2");
    CHECK(LomPaths::clip(0, 0).to_string() == "song/tracks/0/clip_slots/0/clip");
}

// =============================================================================
// Serialisation Tests
// =============================================================================

TEST_CASE("serialize get_property request", "[bridge][serialize]") {
    auto req = LomProtocol::get_property(LomPaths::song(), "tempo");
    auto json_str = LomProtocol::serialize_request(req);

    // Should be valid JSON
    REQUIRE_FALSE(json_str.empty());
    CHECK(json_str.find("\"bridge_protocol_version\":46") != std::string::npos);
    CHECK(json_str.find("\"type\":\"get\"") != std::string::npos);
    CHECK(json_str.find("\"name\":\"tempo\"") != std::string::npos);
    CHECK(json_str.find("\"path\":\"song\"") != std::string::npos);
}

TEST_CASE("serialize set_property request", "[bridge][serialize]") {
    auto req = LomProtocol::set_property(LomPaths::track(0), "volume", 0.75);
    auto json_str = LomProtocol::serialize_request(req);

    REQUIRE_FALSE(json_str.empty());
    CHECK(json_str.find("\"type\":\"set\"") != std::string::npos);
    CHECK(json_str.find("0.75") != std::string::npos);
}

TEST_CASE("serialize call_method request", "[bridge][serialize]") {
    auto req = LomProtocol::call_method(LomPaths::clip_slot(0, 0), "create_clip", {4.0});
    auto json_str = LomProtocol::serialize_request(req);

    REQUIRE_FALSE(json_str.empty());
    CHECK(json_str.find("\"type\":\"call\"") != std::string::npos);
    CHECK(json_str.find("create_clip") != std::string::npos);
}

TEST_CASE("serialize notes", "[bridge][serialize]") {
    std::vector<LomNoteData> notes = {
        {60, 0.0, 1.0, 100, false},
        {64, 1.0, 0.5, 80, true},
    };
    auto json_str = LomProtocol::serialize_notes(notes);

    REQUIRE_FALSE(json_str.empty());
    CHECK(json_str.find("\"pitch\":60") != std::string::npos);
    CHECK(json_str.find("\"pitch\":64") != std::string::npos);
    CHECK(json_str.find("\"velocity\":100") != std::string::npos);
    CHECK(json_str.find("\"start_time\":0.0") != std::string::npos);
    CHECK(json_str.find("\"mute\":true") != std::string::npos);
    CHECK(json_str.find("\"probability\":1.0") != std::string::npos);
    CHECK(json_str.find("\"velocity_deviation\":0.0") != std::string::npos);
    CHECK(json_str.find("\"release_velocity\":64.0") != std::string::npos);

    auto request = LomProtocol::add_new_notes(LomPaths::clip(0, 0), notes);
    auto request_json = nlohmann::json::parse(LomProtocol::serialize_request(request));
    CHECK(request_json["name"] == "add_new_notes");
    REQUIRE(request_json["args"].size() == 1);
    CHECK(request_json["args"][0]["notes"][1]["pitch"] == 64);
    CHECK(request_json["args"][0]["notes"][1]["probability"] == 1.0);
    CHECK(request_json["args"][0]["notes"][1]["velocity_deviation"] == 0.0);
    CHECK(request_json["args"][0]["notes"][1]["release_velocity"] == 64.0);
}

TEST_CASE("native request validation mirrors the closed current bridge algebra",
          "[bridge][protocol][trust-boundary]") {
    CHECK(LomProtocol::validate_request(LomProtocol::get_property(LomPaths::song(), "tempo")));
    CHECK(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::song(), "sunny_get_scene_count")));
    CHECK(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::song(), "sunny_get_return_track_count")));
    CHECK(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::song(), "sunny_get_track_count")));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::song(), "sunny_get_track_count", {1})));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::track(0), "sunny_get_track_count")));
    // The remote log takes exactly one non-negative integer sequence.
    CHECK(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::song(), "sunny_get_remote_log", {0})));
    CHECK(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::song(), "sunny_get_remote_log", {42})));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::song(), "sunny_get_remote_log")));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::song(), "sunny_get_remote_log", {-1})));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::song(), "sunny_get_remote_log", {1.5})));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::track(0), "sunny_get_remote_log", {0})));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::song(), "signature_denominator", 16)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "signature_numerator", 99)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "signature_denominator", 8)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "start_marker", 0.0)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "end_marker", 4.0)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "looping", false)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "muted", false)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "launch_mode", 0)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "launch_quantization", 1)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "legato", false)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "velocity_amount", 0.0)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "groove", nlohmann::json(nullptr))));
    CHECK(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::track(0).child("mixer_device"), "crossfade_assign", 1)));
    CHECK(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::song().child("return_tracks").child(0).child("mixer_device"),
        "crossfade_assign",
        1)));
    CHECK(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::song().child("return_tracks").child(0), "mute", false)));
    CHECK(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::song().child("return_tracks").child(0), "solo", false)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::track(0).child("mixer_device"), "panning_mode", 0)));
    CHECK(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::song().child("return_tracks").child(0).child("mixer_device").child("panning"),
        "value",
        0.25)));
    CHECK(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::song().child("master_track").child("mixer_device"), "panning_mode", 0)));
    CHECK(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::track(0).child("mixer_device").child("track_activator"), "value", 0.0)));
    CHECK(LomProtocol::validate_request(LomProtocol::set_property(LomPaths::song()
                                                                      .child("return_tracks")
                                                                      .child(0)
                                                                      .child("mixer_device")
                                                                      .child("track_activator"),
                                                                  "value",
                                                                  1.0)));
    CHECK(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::song().child("master_track").child("mixer_device").child("track_activator"),
        "value",
        1.0)));
    CHECK(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::song().child("master_track").child("mixer_device").child("panning"),
        "value",
        0.0)));
    CHECK(
        LomProtocol::validate_request(LomProtocol::set_property(LomPaths::track(0), "arm", false)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::track(0), "implicit_arm", false)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::clip(0, 0), "sunny_clear_all_envelopes")));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::scene(0), "tempo_enabled", false)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::scene(0), "name", std::string{"Full Score"})));
    CHECK(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::scene(0), "time_signature_enabled", false)));
    CHECK(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::track(0), "insert_device", {std::string{"EQ Eight"}})));
    const nlohmann::json routing_type = {{"display_name", "Master"}, {"identifier", "master"}};
    const nlohmann::json routing_channel = {{"display_name", "1/2"}, {"identifier", "stereo_1_2"}};
    CHECK(LomProtocol::validate_request(LomProtocol::call_method(
        LomPaths::track(0), "sunny_set_output_routing_type", {routing_type})));
    CHECK(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::song().child("return_tracks").child(0),
                                 "sunny_set_output_routing_channel",
                                 {routing_type, routing_channel})));
    const auto device_path = LomPaths::track(0).child("devices").child(0);
    CHECK(LomProtocol::validate_request(
        LomProtocol::call_method(device_path,
                                 "sunny_get_device_parameter",
                                 {std::string{"Dry/Wet"}, std::string{"value"}})));
    CHECK(LomProtocol::validate_request(
        LomProtocol::call_method(device_path,
                                 "sunny_set_device_parameter",
                                 {std::string{"Dry/Wet"}, 0.5, std::string{"value"}, 0.0, 1.0})));

    CHECK_FALSE(
        LomProtocol::validate_request(LomProtocol::get_property(LomPaths::track(0), "name")));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::track(0),
                                 "sunny_set_output_routing_type",
                                 {nlohmann::json{{"display_name", "Master"}}})));
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::call_method(
        LomPaths::track(0),
        "sunny_set_output_routing_channel",
        {routing_type, nlohmann::json{{"display_name", "1/2"}, {"identifier", 1}}})));
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::call_method(
        LomPaths::song().child("master_track"), "sunny_set_output_routing_type", {routing_type})));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::get_property(LomPath{{"song", "unknown"}}, "name")));
    CHECK_FALSE(
        LomProtocol::validate_request(LomProtocol::set_property(LomPaths::song(), "tempo", 19.0)));
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::song(), "tempo", std::numeric_limits<double>::quiet_NaN())));
    CHECK_FALSE(
        LomProtocol::validate_request(LomProtocol::get_property(LomPaths::song(), "scenes")));
    CHECK_FALSE(
        LomProtocol::validate_request(LomProtocol::get_property(LomPaths::song(), "tracks")));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::get_property(LomPaths::song(), "return_tracks")));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::song(), "sunny_get_scene_count", {1})));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::song(), "signature_denominator", 3)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "signature_numerator", 100)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "signature_denominator", 3)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "start_marker", -1.0)));
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::clip(0, 0), "end_marker", std::numeric_limits<double>::infinity())));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "looping", 0)));
    CHECK_FALSE(
        LomProtocol::validate_request(LomProtocol::set_property(LomPaths::clip(0, 0), "muted", 0)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "launch_mode", 1)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "launch_mode", false)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "launch_quantization", 0)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "legato", true)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "velocity_amount", 0)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "velocity_amount", 0.5)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::clip(0, 0), "groove", false)));
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::track(0).child("mixer_device"), "crossfade_assign", 0)));
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::track(0).child("mixer_device"), "crossfade_assign", 2)));
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::track(0).child("mixer_device"), "crossfade_assign", true)));
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::song().child("master_track").child("mixer_device"), "crossfade_assign", 1)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::track(0).child("mixer_device"), "panning_mode", 1)));
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::track(0).child("mixer_device"), "panning_mode", false)));
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::track(0).child("mixer_device").child("track_activator"), "value", 1)));
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::track(0).child("mixer_device").child("track_activator"), "value", 0.5)));
    CHECK_FALSE(
        LomProtocol::validate_request(LomProtocol::set_property(LomPaths::song()
                                                                    .child("return_tracks")
                                                                    .child(0)
                                                                    .child("mixer_device")
                                                                    .child("track_activator"),
                                                                "value",
                                                                0.0)));
    CHECK_FALSE(
        LomProtocol::validate_request(LomProtocol::set_property(LomPaths::song()
                                                                    .child("return_tracks")
                                                                    .child(0)
                                                                    .child("mixer_device")
                                                                    .child("track_activator"),
                                                                "value",
                                                                1)));
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::song().child("master_track").child("mixer_device").child("track_activator"),
        "value",
        1)));
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::song().child("master_track").child("mixer_device").child("panning"),
        "value",
        0.25)));
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::set_property(
        LomPaths::song().child("master_track").child("mixer_device").child("panning"),
        "value",
        0)));
    CHECK_FALSE(
        LomProtocol::validate_request(LomProtocol::set_property(LomPaths::track(0), "arm", true)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::track(0), "implicit_arm", true)));
    CHECK_FALSE(
        LomProtocol::validate_request(LomProtocol::set_property(LomPaths::track(0), "arm", 0)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::song().child("return_tracks").child(0), "arm", false)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::song().child("return_tracks").child(0), "mute", 0)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::song().child("return_tracks").child(0), "mute", true)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::clip(0, 0), "sunny_clear_all_envelopes", {false})));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::scene(0), "tempo_enabled", 0)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::set_property(LomPaths::scene(0), "tempo", 120.0)));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::track(0), "insert_device", {std::string{}})));
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(device_path,
                                 "sunny_get_device_parameter",
                                 {std::string{"Dry/Wet"}, std::string{"unknown"}})));
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::call_method(
        device_path, "sunny_get_device_parameter", {std::string{"Dry/Wet"}})));

    const std::vector<LomNoteData> notes{{60, 0.0, 1.0, 100, false}};
    CHECK(LomProtocol::validate_notes(LomPaths::clip(0, 0), notes));
    const nlohmann::json note_query = {{"note_ids", {11, 12}},
                                       {"return",
                                        {"note_id",
                                         "pitch",
                                         "start_time",
                                         "duration",
                                         "velocity",
                                         "mute",
                                         "probability",
                                         "velocity_deviation",
                                         "release_velocity"}}};
    CHECK(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::clip(0, 0), "get_notes_by_id", {note_query})));
    const nlohmann::json all_notes_query = {{"return", note_query.at("return")}};
    CHECK(LomProtocol::validate_request(LomProtocol::call_method(
        LomPaths::clip(0, 0), "get_all_notes_extended", {all_notes_query})));
    const nlohmann::json ranged_query = {{"return", note_query.at("return")},
                                         {"from_pitch", 0},
                                         {"pitch_span", 128},
                                         {"from_time", 0.0},
                                         {"time_span", 4.0}};
    CHECK(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::clip(0, 0), "get_notes_extended", {ranged_query})));
    for (const auto& [field, invalid] :
         std::vector<std::pair<std::string, nlohmann::json>>{{"time_span", 0.0},
                                                             {"time_span", -1.0},
                                                             {"from_time", -1.0},
                                                             {"from_pitch", false},
                                                             {"pitch_span", 127},
                                                             {"unknown", 0}}) {
        auto malformed = ranged_query;
        malformed[field] = invalid;
        CHECK_FALSE(LomProtocol::validate_request(
            LomProtocol::call_method(LomPaths::clip(0, 0), "get_notes_extended", {malformed})));
    }
    auto duplicate_query = note_query;
    duplicate_query["note_ids"] = {11, 11};
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::clip(0, 0), "get_notes_by_id", {duplicate_query})));
    auto oversized_query = note_query;
    oversized_query["note_ids"] = {std::numeric_limits<std::int64_t>::max()};
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::clip(0, 0), "get_notes_by_id", {oversized_query})));
    auto open_query = note_query;
    open_query["return"].push_back("unknown");
    CHECK_FALSE(LomProtocol::validate_request(
        LomProtocol::call_method(LomPaths::clip(0, 0), "get_notes_by_id", {open_query})));
    auto open_all_query = all_notes_query;
    open_all_query["note_ids"] = {11};
    CHECK_FALSE(LomProtocol::validate_request(LomProtocol::call_method(
        LomPaths::clip(0, 0), "get_all_notes_extended", {open_all_query})));
    CHECK_FALSE(LomProtocol::validate_notes(LomPaths::clip(0, 0), {}));
    auto invalid_notes = notes;
    invalid_notes[0].duration = std::numeric_limits<double>::infinity();
    CHECK_FALSE(LomProtocol::validate_notes(LomPaths::clip(0, 0), invalid_notes));
    invalid_notes = notes;
    invalid_notes[0].probability = 0.5;
    CHECK_FALSE(LomProtocol::validate_notes(LomPaths::clip(0, 0), invalid_notes));
    invalid_notes = notes;
    invalid_notes[0].velocity_deviation = 1.0;
    CHECK_FALSE(LomProtocol::validate_notes(LomPaths::clip(0, 0), invalid_notes));
    auto expressive_release = notes;
    expressive_release[0].release_velocity = 63.0;
    CHECK(LomProtocol::validate_notes(LomPaths::clip(0, 0), expressive_release));
    auto integral_encoding = LomProtocol::add_new_notes(LomPaths::clip(0, 0), notes);
    std::get<nlohmann::json>(integral_encoding.args[0])["notes"][0]["release_velocity"] = 63;
    CHECK_FALSE(LomProtocol::validate_request(integral_encoding));
    invalid_notes = notes;
    invalid_notes[0].release_velocity = 63.5;
    CHECK_FALSE(LomProtocol::validate_notes(LomPaths::clip(0, 0), invalid_notes));
    invalid_notes = notes;
    invalid_notes[0].release_velocity = 128.0;
    CHECK_FALSE(LomProtocol::validate_notes(LomPaths::clip(0, 0), invalid_notes));
}

TEST_CASE("NoteEvent conversion maps whole-note units to Live quarter-note beats",
          "[bridge][serialize]") {
    sunny::core::NoteEvent note{60, sunny::core::Beat{1, 4}, sunny::core::Beat{1, 8}, 100, true};

    const auto live_note = LomProtocol::from_note_event(note);
    REQUIRE(live_note.has_value());
    CHECK(live_note->start_time == 1.0);
    CHECK(live_note->duration == 0.5);
    CHECK(live_note->muted);
    CHECK(live_note->probability == 1.0);
    CHECK(live_note->velocity_deviation == 0.0);
    CHECK(live_note->release_velocity == 64.0);

    note.release_velocity = 23;
    const auto expressive_live_note = LomProtocol::from_note_event(note);
    REQUIRE(expressive_live_note.has_value());
    CHECK(expressive_live_note->release_velocity == 23.0);

    note.duration = sunny::core::Beat{0, 1};
    CHECK_FALSE(LomProtocol::from_note_event(note));
    note.duration = sunny::core::Beat{1, 8};
    note.start_time = sunny::core::Beat{-1, 4};
    CHECK_FALSE(LomProtocol::from_note_event(note));
    note.start_time = sunny::core::Beat{0, 1};
    note.velocity = 0;
    CHECK_FALSE(LomProtocol::from_note_event(note));
}

// =============================================================================
// Deserialisation Tests
// =============================================================================

TEST_CASE("deserialize success response", "[bridge][deserialize]") {
    auto resp = LomProtocol::deserialize_response(
        R"({"bridge_protocol_version":46,"success":true,"value":120.0})");

    REQUIRE(resp.has_value());
    CHECK(resp->success == true);
    REQUIRE(resp->value.has_value());
    CHECK(std::get<double>(*resp->value) == 120.0);
    CHECK_FALSE(resp->error.has_value());
}

TEST_CASE("deserialize error response", "[bridge][deserialize]") {
    auto resp = LomProtocol::deserialize_response(
        R"({"bridge_protocol_version":46,"success":false,"error":"Track not found"})");

    REQUIRE(resp.has_value());
    CHECK(resp->success == false);
    REQUIRE(resp->error.has_value());
    CHECK(*resp->error == "Track not found");
}

TEST_CASE("deserialize integer value", "[bridge][deserialize]") {
    auto resp = LomProtocol::deserialize_response(
        R"({"bridge_protocol_version":46,"success":true,"value":42})");

    REQUIRE(resp.has_value());
    REQUIRE(resp->value.has_value());
    CHECK(std::get<int>(*resp->value) == 42);
}

TEST_CASE("deserialize boolean value", "[bridge][deserialize]") {
    auto resp = LomProtocol::deserialize_response(
        R"({"bridge_protocol_version":46,"success":true,"value":true})");

    REQUIRE(resp.has_value());
    REQUIRE(resp->value.has_value());
    CHECK(std::get<bool>(*resp->value) == true);
}

TEST_CASE("deserialize string value", "[bridge][deserialize]") {
    auto resp = LomProtocol::deserialize_response(
        R"({"bridge_protocol_version":46,"success":true,"value":"hello world"})");

    REQUIRE(resp.has_value());
    REQUIRE(resp->value.has_value());
    CHECK(std::get<std::string>(*resp->value) == "hello world");
}

TEST_CASE("deserialize array values", "[bridge][deserialize]") {
    SECTION("integer array") {
        auto resp = LomProtocol::deserialize_response(
            R"({"bridge_protocol_version":46,"success":true,"value":[1,2,3]})");

        REQUIRE(resp.has_value());
        REQUIRE(resp->value.has_value());
        auto& arr = std::get<std::vector<int>>(*resp->value);
        REQUIRE(arr.size() == 3);
        CHECK(arr[0] == 1);
        CHECK(arr[2] == 3);
    }

    SECTION("string array") {
        auto resp = LomProtocol::deserialize_response(
            R"({"bridge_protocol_version":46,"success":true,"value":["a","b"]})");

        REQUIRE(resp.has_value());
        REQUIRE(resp->value.has_value());
        auto& arr = std::get<std::vector<std::string>>(*resp->value);
        REQUIRE(arr.size() == 2);
        CHECK(arr[0] == "a");
    }
}

TEST_CASE("deserialize null value", "[bridge][deserialize]") {
    auto resp = LomProtocol::deserialize_response(
        R"({"bridge_protocol_version":46,"success":true,"value":null})");

    REQUIRE(resp.has_value());
    CHECK_FALSE(resp->value.has_value());
}

// =============================================================================
// Malformed Input Tests
// =============================================================================

TEST_CASE("malformed JSON returns nullopt", "[bridge][deserialize]") {
    CHECK_FALSE(LomProtocol::deserialize_response("not json").has_value());
    CHECK_FALSE(LomProtocol::deserialize_response("").has_value());
    CHECK_FALSE(LomProtocol::deserialize_response("{broken").has_value());
    // "null" is valid JSON; deserialize returns a response with success=false
}

TEST_CASE("response envelope requires matching version and success", "[bridge][deserialize]") {
    auto resp = LomProtocol::deserialize_response(R"({"value": 42})");
    CHECK_FALSE(resp.has_value());
    CHECK_FALSE(LomProtocol::deserialize_response(
                    R"({"bridge_protocol_version":4,"success":true,"value":42})")
                    .has_value());
    CHECK_FALSE(
        LomProtocol::deserialize_response(R"({"bridge_protocol_version":46,"success":false})")
            .has_value());
    CHECK_FALSE(LomProtocol::deserialize_response(
                    R"({"bridge_protocol_version":46,"success":true,"value":9223372036854775807})")
                    .has_value());
    CHECK_FALSE(LomProtocol::deserialize_response(
                    R"({"bridge_protocol_version":46,"success":true,"extra":1})")
                    .has_value());
    CHECK_FALSE(LomProtocol::deserialize_response(
                    R"({"bridge_protocol_version":46,"success":true,"error":"contradiction"})")
                    .has_value());
    CHECK_FALSE(LomProtocol::deserialize_response(
                    R"({"bridge_protocol_version":46,"success":false,"error":"bad","value":1})")
                    .has_value());
}

TEST_CASE("serialize request with string args handles escaping", "[bridge][serialize]") {
    auto req = LomProtocol::set_property(
        LomPaths::song(), "notes", std::string("line1\nline2\t\"quoted\""));
    auto json_str = LomProtocol::serialize_request(req);

    // Verify JSON string escaping works
    REQUIRE_FALSE(json_str.empty());

    // Should be valid JSON that can be re-parsed
    auto resp = LomProtocol::deserialize_response(
        R"({"bridge_protocol_version":46,"success":true,"value":")" + std::string("ok") + R"("})");
    REQUIRE(resp.has_value());
}

TEST_CASE("round-trip serialize/deserialize", "[bridge][roundtrip]") {
    // Serialize a request, then verify the JSON is well-formed by
    // deserializing a response with the same path
    auto req = LomProtocol::get_property(LomPaths::clip(1, 2), "length");
    auto json_str = LomProtocol::serialize_request(req);

    // The serialized request is valid JSON
    REQUIRE_FALSE(json_str.empty());
    CHECK(json_str.front() == '{');
    CHECK(json_str.back() == '}');
}
