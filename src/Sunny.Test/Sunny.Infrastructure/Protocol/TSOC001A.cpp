/**
 * @file TSOC001A.cpp
 * @brief Unit tests for WPOSC001A (OSC Wire Protocol Codec)
 *
 * Component: TSOC001A
 * Domain: TS (Test) | Category: OC (OSC Codec)
 *
 * Tests: WPOSC001A
 * Coverage: encode/decode round-trip, alignment, boundary values,
 *           malformed input handling
 */

#include <catch2/catch_test_macros.hpp>

#include "Protocol/WPOSC001A.h"

#include <array>
#include <cstring>

using namespace Sunny::Infrastructure;

// =============================================================================
// Writer Tests
// =============================================================================

TEST_CASE("WPOSC001A: encode empty message", "[osc][codec]") {
    std::array<std::byte, 256> buf{};
    OscWriter w(buf);

    w.begin_message("/test").end_message();
    REQUIRE_FALSE(w.has_error());

    auto pkt = w.packet();
    REQUIRE(pkt.size() > 0);
    REQUIRE(pkt.size() % 4 == 0);  // 4-byte aligned
}

TEST_CASE("WPOSC001A: encode int32 argument", "[osc][codec]") {
    std::array<std::byte, 256> buf{};
    OscWriter w(buf);

    w.begin_message("/value")
     .add_int32(42)
     .end_message();
    REQUIRE_FALSE(w.has_error());

    auto pkt = w.packet();
    REQUIRE(pkt.size() % 4 == 0);
}

TEST_CASE("WPOSC001A: encode float32 argument", "[osc][codec]") {
    std::array<std::byte, 256> buf{};
    OscWriter w(buf);

    w.begin_message("/tempo")
     .add_float32(120.5f)
     .end_message();
    REQUIRE_FALSE(w.has_error());
}

TEST_CASE("WPOSC001A: encode string argument", "[osc][codec]") {
    std::array<std::byte, 256> buf{};
    OscWriter w(buf);

    w.begin_message("/name")
     .add_string("hello")
     .end_message();
    REQUIRE_FALSE(w.has_error());

    auto pkt = w.packet();
    REQUIRE(pkt.size() % 4 == 0);
}

TEST_CASE("WPOSC001A: encode multiple arguments", "[osc][codec]") {
    std::array<std::byte, 512> buf{};
    OscWriter w(buf);

    w.begin_message("/multi")
     .add_int32(1)
     .add_float32(2.5f)
     .add_string("three")
     .add_int32(4)
     .end_message();
    REQUIRE_FALSE(w.has_error());
}

TEST_CASE("WPOSC001A: encode blob argument", "[osc][codec]") {
    std::array<std::byte, 256> buf{};
    OscWriter w(buf);

    std::array<std::byte, 5> blob_data{
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03},
        std::byte{0x04}, std::byte{0x05}
    };

    w.begin_message("/blob")
     .add_blob(blob_data)
     .end_message();
    REQUIRE_FALSE(w.has_error());

    auto pkt = w.packet();
    REQUIRE(pkt.size() % 4 == 0);
}

TEST_CASE("WPOSC001A: buffer overflow sets error", "[osc][codec]") {
    std::array<std::byte, 8> tiny_buf{};
    OscWriter w(tiny_buf);

    w.begin_message("/this/address/is/way/too/long/for/the/buffer")
     .end_message();
    CHECK(w.has_error());
}

// =============================================================================
// Reader Tests
// =============================================================================

TEST_CASE("WPOSC001A: decode empty message", "[osc][codec]") {
    std::array<std::byte, 256> buf{};
    OscWriter w(buf);

    w.begin_message("/empty").end_message();
    auto pkt = w.packet();

    OscReader r(pkt);
    REQUIRE_FALSE(r.has_error());
    CHECK(r.address() == "/empty");
    CHECK(r.arguments().empty());
}

TEST_CASE("WPOSC001A: malformed packet too short", "[osc][codec]") {
    std::array<std::byte, 2> tiny{std::byte{0x2F}, std::byte{0x00}};
    OscReader r(std::span<const std::byte>(tiny.data(), tiny.size()));
    // Should not crash; may or may not set error depending on implementation
    // At minimum, address parsing should handle gracefully
}

// =============================================================================
// Round-trip Tests
// =============================================================================

TEST_CASE("WPOSC001A: int32 round-trip", "[osc][codec]") {
    std::array<std::byte, 256> buf{};
    OscWriter w(buf);

    w.begin_message("/test/int")
     .add_int32(-42)
     .add_int32(0)
     .add_int32(2147483647)
     .end_message();
    REQUIRE_FALSE(w.has_error());

    OscReader r(w.packet());
    REQUIRE_FALSE(r.has_error());
    CHECK(r.address() == "/test/int");

    auto& args = r.arguments();
    REQUIRE(args.size() == 3);

    CHECK(args[0].type == OscArgument::Type::Int32);
    CHECK(args[0].int_value == -42);

    CHECK(args[1].type == OscArgument::Type::Int32);
    CHECK(args[1].int_value == 0);

    CHECK(args[2].type == OscArgument::Type::Int32);
    CHECK(args[2].int_value == 2147483647);
}

TEST_CASE("WPOSC001A: float32 round-trip", "[osc][codec]") {
    std::array<std::byte, 256> buf{};
    OscWriter w(buf);

    w.begin_message("/test/float")
     .add_float32(0.0f)
     .add_float32(120.5f)
     .add_float32(-1.0f)
     .end_message();
    REQUIRE_FALSE(w.has_error());

    OscReader r(w.packet());
    REQUIRE_FALSE(r.has_error());

    auto& args = r.arguments();
    REQUIRE(args.size() == 3);

    CHECK(args[0].type == OscArgument::Type::Float32);
    CHECK(args[0].float_value == 0.0f);

    CHECK(args[1].type == OscArgument::Type::Float32);
    CHECK(args[1].float_value == 120.5f);

    CHECK(args[2].type == OscArgument::Type::Float32);
    CHECK(args[2].float_value == -1.0f);
}

TEST_CASE("WPOSC001A: string round-trip", "[osc][codec]") {
    std::array<std::byte, 256> buf{};
    OscWriter w(buf);

    w.begin_message("/test/string")
     .add_string("hello")
     .add_string("")
     .add_string("ab")
     .end_message();
    REQUIRE_FALSE(w.has_error());

    OscReader r(w.packet());
    REQUIRE_FALSE(r.has_error());

    auto& args = r.arguments();
    REQUIRE(args.size() == 3);

    CHECK(args[0].type == OscArgument::Type::String);
    CHECK(args[0].string_value == "hello");

    CHECK(args[1].type == OscArgument::Type::String);
    CHECK(args[1].string_value == "");

    CHECK(args[2].type == OscArgument::Type::String);
    CHECK(args[2].string_value == "ab");
}

TEST_CASE("WPOSC001A: mixed types round-trip", "[osc][codec]") {
    std::array<std::byte, 512> buf{};
    OscWriter w(buf);

    w.begin_message("/live/clip/add/notes")
     .add_int32(0)
     .add_int32(1)
     .add_int32(60)
     .add_float32(0.0f)
     .add_float32(1.0f)
     .add_int32(100)
     .end_message();
    REQUIRE_FALSE(w.has_error());

    OscReader r(w.packet());
    REQUIRE_FALSE(r.has_error());
    CHECK(r.address() == "/live/clip/add/notes");

    auto& args = r.arguments();
    REQUIRE(args.size() == 6);

    CHECK(args[0].int_value == 0);
    CHECK(args[1].int_value == 1);
    CHECK(args[2].int_value == 60);
    CHECK(args[3].float_value == 0.0f);
    CHECK(args[4].float_value == 1.0f);
    CHECK(args[5].int_value == 100);
}

TEST_CASE("WPOSC001A: blob round-trip", "[osc][codec]") {
    std::array<std::byte, 256> buf{};
    OscWriter w(buf);

    std::array<std::byte, 7> blob_data{
        std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE},
        std::byte{0xEF}, std::byte{0xCA}, std::byte{0xFE},
        std::byte{0x42}
    };

    w.begin_message("/test/blob")
     .add_blob(blob_data)
     .end_message();
    REQUIRE_FALSE(w.has_error());

    OscReader r(w.packet());
    REQUIRE_FALSE(r.has_error());

    auto& args = r.arguments();
    REQUIRE(args.size() == 1);
    CHECK(args[0].type == OscArgument::Type::Blob);
    CHECK(args[0].blob_value.size() == 7);
    CHECK(args[0].blob_value[0] == std::byte{0xDE});
    CHECK(args[0].blob_value[6] == std::byte{0x42});
}

TEST_CASE("WPOSC001A: type tag string is correct", "[osc][codec]") {
    std::array<std::byte, 256> buf{};
    OscWriter w(buf);

    w.begin_message("/tags")
     .add_int32(1)
     .add_float32(2.0f)
     .add_string("s")
     .end_message();

    OscReader r(w.packet());
    REQUIRE_FALSE(r.has_error());
    CHECK(r.type_tag() == ",ifs");
}
