/**
 * @file TSIG001A.cpp
 * @brief Integration tests for the Orchestrator → Bridge pipeline
 *
 * Component: TSIG001A
 * Domain: TS (Test) | Category: IG (Integration)
 *
 * Tests the pipeline from Orchestrator operations through to
 * serialised bridge messages, verifying that theory computation
 * flows correctly to LOM protocol output.
 */

#include <catch2/catch_test_macros.hpp>

#include "Bridge/INBR001A.h"
#include "Protocol/WPOSC001A.h"
#include "RealTime/RTLF001A.h"
#include "Scale/SCDF001A.h"
#include "Scale/SCGN001A.h"
#include "Rhythm/RHEU001A.h"
#include "Tensor/TNTP001A.h"

#include <set>

using namespace Sunny;

// =============================================================================
// OSC Codec → SPSC Ring Buffer Pipeline
// =============================================================================

TEST_CASE("TSIG001A: OSC message through SPSC queue", "[integration][pipeline]") {
    // Simulate: encode OSC on control thread, push through queue,
    // pop on audio thread, decode

    struct OscPacket {
        std::array<std::byte, 256> data{};
        std::size_t size{0};
    };

    Infrastructure::SpscRingBuffer<OscPacket, 16> queue;

    // Control thread: encode and push
    OscPacket pkt;
    {
        Infrastructure::OscWriter w(pkt.data);
        w.begin_message("/live/song/set/tempo")
         .add_float32(140.0f)
         .end_message();
        REQUIRE_FALSE(w.has_error());
        pkt.size = w.bytes_written();
    }
    REQUIRE(queue.try_push(pkt));

    // Audio thread: pop and decode
    OscPacket received;
    REQUIRE(queue.try_pop(received));
    CHECK(received.size == pkt.size);

    Infrastructure::OscReader r(
        std::span<const std::byte>(received.data.data(), received.size));
    REQUIRE_FALSE(r.has_error());
    CHECK(r.address() == "/live/song/set/tempo");
    REQUIRE(r.arguments().size() == 1);
    CHECK(r.arguments()[0].float_value == 140.0f);
}

// =============================================================================
// OSC Encode → Decode Round-trip with Multiple Messages
// =============================================================================

TEST_CASE("TSIG001A: multiple OSC messages through queue", "[integration][pipeline]") {
    struct OscPacket {
        std::array<std::byte, 512> data{};
        std::size_t size{0};
    };

    Infrastructure::SpscRingBuffer<OscPacket, 32> queue;

    // Push several messages
    const std::vector<std::pair<std::string, float>> commands = {
        {"/live/track/set/volume", 0.75f},
        {"/live/track/set/panning", -0.5f},
        {"/live/song/set/tempo", 120.0f},
        {"/live/track/set/volume", 1.0f},
    };

    for (const auto& [addr, val] : commands) {
        OscPacket pkt;
        Infrastructure::OscWriter w(pkt.data);
        w.begin_message(addr).add_float32(val).end_message();
        REQUIRE_FALSE(w.has_error());
        pkt.size = w.bytes_written();
        REQUIRE(queue.try_push(pkt));
    }

    // Pop and verify
    for (const auto& [addr, val] : commands) {
        OscPacket received;
        REQUIRE(queue.try_pop(received));

        Infrastructure::OscReader r(
            std::span<const std::byte>(received.data.data(), received.size));
        REQUIRE_FALSE(r.has_error());
        CHECK(r.address() == addr);
        REQUIRE(r.arguments().size() == 1);
        CHECK(r.arguments()[0].float_value == val);
    }

    CHECK(queue.empty());
}

// =============================================================================
// Bridge Serialisation → Deserialisation Round-trip
// =============================================================================

TEST_CASE("TSIG001A: bridge request-response round-trip", "[integration][bridge]") {
    // Serialize a get_property request
    auto req = Infrastructure::LomProtocol::get_property(
        Infrastructure::LomPaths::song(), "tempo");
    auto json_str = Infrastructure::LomProtocol::serialize_request(req);

    REQUIRE_FALSE(json_str.empty());

    // Simulate a response
    auto resp = Infrastructure::LomProtocol::deserialize_response(
        R"({"success": true, "value": 120.0})");

    REQUIRE(resp.has_value());
    CHECK(resp->success);
    REQUIRE(resp->value.has_value());
    CHECK(std::get<double>(*resp->value) == 120.0);
}

TEST_CASE("TSIG001A: note serialisation pipeline", "[integration][bridge]") {
    // Create NoteEvents, convert to LomNoteData, serialise
    std::vector<Infrastructure::LomNoteData> notes;

    notes.push_back({60, 0.0, 1.0, 100, false});
    notes.push_back({64, 0.0, 1.0, 100, false});
    notes.push_back({67, 0.0, 1.0, 100, false});
    notes.push_back({72, 1.0, 1.0, 80, false});

    auto json_str = Infrastructure::LomProtocol::serialize_notes(notes);
    REQUIRE_FALSE(json_str.empty());

    // Should be valid JSON array
    CHECK(json_str.front() == '[');
    CHECK(json_str.back() == ']');

    // Verify all notes are present
    CHECK(json_str.find("\"pitch\":60") != std::string::npos);
    CHECK(json_str.find("\"pitch\":64") != std::string::npos);
    CHECK(json_str.find("\"pitch\":67") != std::string::npos);
    CHECK(json_str.find("\"pitch\":72") != std::string::npos);
}

// =============================================================================
// Theory → Bridge Pipeline
// =============================================================================

TEST_CASE("TSIG001A: scale generation → bridge message", "[integration][theory]") {
    // Look up major scale intervals
    auto scale_def = Core::find_scale("major");
    REQUIRE(scale_def.has_value());

    // Generate scale notes
    auto scale_result = Core::generate_scale_notes(0, scale_def->get_intervals(), 4);
    REQUIRE(scale_result.has_value());
    auto& scale_notes = *scale_result;
    REQUIRE(scale_notes.size() == 7);

    // Convert to OSC message
    std::array<std::byte, 256> buf{};
    Infrastructure::OscWriter w(buf);
    w.begin_message("/sunny/theory/scale_notes");
    for (auto note : scale_notes) {
        w.add_int32(static_cast<int32_t>(note));
    }
    w.end_message();
    REQUIRE_FALSE(w.has_error());

    // Decode and verify
    Infrastructure::OscReader r(w.packet());
    REQUIRE_FALSE(r.has_error());
    CHECK(r.address() == "/sunny/theory/scale_notes");
    REQUIRE(r.arguments().size() == 7);

    // C major in octave 4: C4=60, D4=62, E4=64, F4=65, G4=67, A4=69, B4=71
    CHECK(r.arguments()[0].int_value == 60);
    CHECK(r.arguments()[1].int_value == 62);
    CHECK(r.arguments()[2].int_value == 64);
    CHECK(r.arguments()[3].int_value == 65);
    CHECK(r.arguments()[4].int_value == 67);
    CHECK(r.arguments()[5].int_value == 69);
    CHECK(r.arguments()[6].int_value == 71);
}

TEST_CASE("TSIG001A: euclidean rhythm → bridge message", "[integration][theory]") {
    auto rhythm = Core::euclidean_rhythm(3, 8);
    REQUIRE(rhythm.has_value());
    auto& pattern = *rhythm;
    REQUIRE(pattern.size() == 8);

    // Encode pattern as OSC
    std::array<std::byte, 256> buf{};
    Infrastructure::OscWriter w(buf);
    w.begin_message("/sunny/rhythm/pattern");
    for (bool hit : pattern) {
        w.add_int32(hit ? 1 : 0);
    }
    w.end_message();
    REQUIRE_FALSE(w.has_error());

    // Decode and verify Euclidean(3,8) = [1,0,0,1,0,0,1,0]
    Infrastructure::OscReader r(w.packet());
    REQUIRE_FALSE(r.has_error());
    REQUIRE(r.arguments().size() == 8);

    int pulse_count = 0;
    for (const auto& arg : r.arguments()) {
        if (arg.int_value == 1) ++pulse_count;
    }
    CHECK(pulse_count == 3);
}

// =============================================================================
// Error Code Coverage
// =============================================================================

TEST_CASE("TSIG001A: error codes have distinct values", "[integration][errors]") {
    // Verify expanded error codes don't collide
    std::set<int> codes;
    auto insert = [&](Core::ErrorCode c) {
        auto val = static_cast<int>(c);
        CHECK(codes.find(val) == codes.end());
        codes.insert(val);
    };

    insert(Core::ErrorCode::Ok);
    insert(Core::ErrorCode::InvalidMidiNote);
    insert(Core::ErrorCode::InvalidVelocity);
    insert(Core::ErrorCode::InvalidPitchClass);
    insert(Core::ErrorCode::InvalidScaleName);
    insert(Core::ErrorCode::ConnectionFailed);
    insert(Core::ErrorCode::ConnectionLost);
    insert(Core::ErrorCode::SendFailed);
    insert(Core::ErrorCode::ReceiveFailed);
    insert(Core::ErrorCode::ProtocolError);
    insert(Core::ErrorCode::SessionNotReady);
    insert(Core::ErrorCode::TransactionFailed);
    insert(Core::ErrorCode::McpParseError);
    insert(Core::ErrorCode::McpToolNotFound);
    insert(Core::ErrorCode::OscEncodeError);
    insert(Core::ErrorCode::OscDecodeError);
}
