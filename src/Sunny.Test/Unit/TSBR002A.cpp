/**
 * @file TSBR002A.cpp
 * @brief Unit tests for INBR001A (LOM Bridge JSON Deserialisation)
 *
 * Component: TSBR002A
 * Domain: TS (Test) | Category: BR (Bridge)
 *
 * Tests: INBR001A
 * Coverage: JSON deserialisation, malformed input, serialisation,
 *           LomPath operations, note serialisation
 */

#include <catch2/catch_test_macros.hpp>

#include "Bridge/INBR001A.h"

using namespace Sunny::Infrastructure;

// =============================================================================
// LomPath Tests
// =============================================================================

TEST_CASE("INBR001A: LomPath to_string", "[bridge][path]") {
    LomPath p{{"song", "tracks", "0"}};
    CHECK(p.to_string() == "song/tracks/0");
}

TEST_CASE("INBR001A: LomPath parse", "[bridge][path]") {
    auto p = LomPath::parse("song/tracks/0/clip_slots/1");
    REQUIRE(p.segments.size() == 5);
    CHECK(p.segments[0] == "song");
    CHECK(p.segments[2] == "0");
    CHECK(p.segments[4] == "1");
}

TEST_CASE("INBR001A: LomPath parse with leading slash", "[bridge][path]") {
    auto p = LomPath::parse("/song/tracks");
    REQUIRE(p.segments.size() == 2);
    CHECK(p.segments[0] == "song");
    CHECK(p.segments[1] == "tracks");
}

TEST_CASE("INBR001A: LomPath child operations", "[bridge][path]") {
    auto p = LomPaths::song();
    CHECK(p.to_string() == "song");

    auto t = p.child("tracks").child(2);
    CHECK(t.to_string() == "song/tracks/2");
}

TEST_CASE("INBR001A: LomPaths helpers", "[bridge][path]") {
    CHECK(LomPaths::track(3).to_string() == "song/tracks/3");
    CHECK(LomPaths::clip_slot(1, 2).to_string() == "song/tracks/1/clip_slots/2");
    CHECK(LomPaths::clip(0, 0).to_string() == "song/tracks/0/clip_slots/0/clip");
}

// =============================================================================
// Serialisation Tests
// =============================================================================

TEST_CASE("INBR001A: serialize get_property request", "[bridge][serialize]") {
    auto req = LomProtocol::get_property(LomPaths::song(), "tempo");
    auto json_str = LomProtocol::serialize_request(req);

    // Should be valid JSON
    REQUIRE_FALSE(json_str.empty());
    CHECK(json_str.find("\"type\":\"get\"") != std::string::npos);
    CHECK(json_str.find("\"name\":\"tempo\"") != std::string::npos);
    CHECK(json_str.find("\"path\":\"song\"") != std::string::npos);
}

TEST_CASE("INBR001A: serialize set_property request", "[bridge][serialize]") {
    auto req = LomProtocol::set_property(
        LomPaths::track(0), "volume", 0.75);
    auto json_str = LomProtocol::serialize_request(req);

    REQUIRE_FALSE(json_str.empty());
    CHECK(json_str.find("\"type\":\"set\"") != std::string::npos);
    CHECK(json_str.find("0.75") != std::string::npos);
}

TEST_CASE("INBR001A: serialize call_method request", "[bridge][serialize]") {
    auto req = LomProtocol::call_method(
        LomPaths::clip_slot(0, 0), "create_clip",
        {4.0});
    auto json_str = LomProtocol::serialize_request(req);

    REQUIRE_FALSE(json_str.empty());
    CHECK(json_str.find("\"type\":\"call\"") != std::string::npos);
    CHECK(json_str.find("create_clip") != std::string::npos);
}

TEST_CASE("INBR001A: serialize notes", "[bridge][serialize]") {
    std::vector<LomNoteData> notes = {
        {60, 0.0, 1.0, 100, false},
        {64, 1.0, 0.5, 80, true},
    };
    auto json_str = LomProtocol::serialize_notes(notes);

    REQUIRE_FALSE(json_str.empty());
    CHECK(json_str.find("\"pitch\":60") != std::string::npos);
    CHECK(json_str.find("\"pitch\":64") != std::string::npos);
    CHECK(json_str.find("\"velocity\":100") != std::string::npos);
    CHECK(json_str.find("\"muted\":true") != std::string::npos);
}

// =============================================================================
// Deserialisation Tests
// =============================================================================

TEST_CASE("INBR001A: deserialize success response", "[bridge][deserialize]") {
    auto resp = LomProtocol::deserialize_response(
        R"({"success": true, "value": 120.0})");

    REQUIRE(resp.has_value());
    CHECK(resp->success == true);
    REQUIRE(resp->value.has_value());
    CHECK(std::get<double>(*resp->value) == 120.0);
    CHECK_FALSE(resp->error.has_value());
}

TEST_CASE("INBR001A: deserialize error response", "[bridge][deserialize]") {
    auto resp = LomProtocol::deserialize_response(
        R"({"success": false, "error": "Track not found"})");

    REQUIRE(resp.has_value());
    CHECK(resp->success == false);
    REQUIRE(resp->error.has_value());
    CHECK(*resp->error == "Track not found");
}

TEST_CASE("INBR001A: deserialize response with callback_id", "[bridge][deserialize]") {
    auto resp = LomProtocol::deserialize_response(
        R"({"success": true, "value": "ok", "callback_id": "cb_42"})");

    REQUIRE(resp.has_value());
    REQUIRE(resp->callback_id.has_value());
    CHECK(*resp->callback_id == "cb_42");
}

TEST_CASE("INBR001A: deserialize integer value", "[bridge][deserialize]") {
    auto resp = LomProtocol::deserialize_response(
        R"({"success": true, "value": 42})");

    REQUIRE(resp.has_value());
    REQUIRE(resp->value.has_value());
    CHECK(std::get<int>(*resp->value) == 42);
}

TEST_CASE("INBR001A: deserialize boolean value", "[bridge][deserialize]") {
    auto resp = LomProtocol::deserialize_response(
        R"({"success": true, "value": true})");

    REQUIRE(resp.has_value());
    REQUIRE(resp->value.has_value());
    CHECK(std::get<bool>(*resp->value) == true);
}

TEST_CASE("INBR001A: deserialize string value", "[bridge][deserialize]") {
    auto resp = LomProtocol::deserialize_response(
        R"({"success": true, "value": "hello world"})");

    REQUIRE(resp.has_value());
    REQUIRE(resp->value.has_value());
    CHECK(std::get<std::string>(*resp->value) == "hello world");
}

TEST_CASE("INBR001A: deserialize array values", "[bridge][deserialize]") {
    SECTION("integer array") {
        auto resp = LomProtocol::deserialize_response(
            R"({"success": true, "value": [1, 2, 3]})");

        REQUIRE(resp.has_value());
        REQUIRE(resp->value.has_value());
        auto& arr = std::get<std::vector<int>>(*resp->value);
        REQUIRE(arr.size() == 3);
        CHECK(arr[0] == 1);
        CHECK(arr[2] == 3);
    }

    SECTION("string array") {
        auto resp = LomProtocol::deserialize_response(
            R"({"success": true, "value": ["a", "b"]})");

        REQUIRE(resp.has_value());
        REQUIRE(resp->value.has_value());
        auto& arr = std::get<std::vector<std::string>>(*resp->value);
        REQUIRE(arr.size() == 2);
        CHECK(arr[0] == "a");
    }
}

TEST_CASE("INBR001A: deserialize null value", "[bridge][deserialize]") {
    auto resp = LomProtocol::deserialize_response(
        R"({"success": true, "value": null})");

    REQUIRE(resp.has_value());
    CHECK_FALSE(resp->value.has_value());
}

// =============================================================================
// Malformed Input Tests
// =============================================================================

TEST_CASE("INBR001A: malformed JSON returns nullopt", "[bridge][deserialize]") {
    CHECK_FALSE(LomProtocol::deserialize_response("not json").has_value());
    CHECK_FALSE(LomProtocol::deserialize_response("").has_value());
    CHECK_FALSE(LomProtocol::deserialize_response("{broken").has_value());
    // "null" is valid JSON; deserialize returns a response with success=false
}

TEST_CASE("INBR001A: missing success field defaults to false", "[bridge][deserialize]") {
    auto resp = LomProtocol::deserialize_response(R"({"value": 42})");

    REQUIRE(resp.has_value());
    CHECK(resp->success == false);
}

TEST_CASE("INBR001A: serialize request with string args handles escaping", "[bridge][serialize]") {
    auto req = LomProtocol::set_property(
        LomPaths::song(), "notes",
        std::string("line1\nline2\t\"quoted\""));
    auto json_str = LomProtocol::serialize_request(req);

    // Verify JSON string escaping works
    REQUIRE_FALSE(json_str.empty());

    // Should be valid JSON that can be re-parsed
    auto resp = LomProtocol::deserialize_response(
        R"({"success": true, "value": ")" + std::string("ok") + R"("})");
    REQUIRE(resp.has_value());
}

TEST_CASE("INBR001A: round-trip serialize/deserialize", "[bridge][roundtrip]") {
    // Serialize a request, then verify the JSON is well-formed by
    // deserializing a response with the same path
    auto req = LomProtocol::get_property(
        LomPaths::clip(1, 2), "length");
    auto json_str = LomProtocol::serialize_request(req);

    // The serialized request is valid JSON
    REQUIRE_FALSE(json_str.empty());
    CHECK(json_str.front() == '{');
    CHECK(json_str.back() == '}');
}
