/**
 * @file TSBR003A.cpp
 * @brief Unit tests for LOM Transport (INTP001A)
 *
 * Component: TSBR003A
 * Domain: TS (Test) | Category: BR (Bridge)
 *
 * Tests: INTP001A
 * Coverage: CommandBuffer recording, query methods, TcpTransport state
 */

#include <catch2/catch_test_macros.hpp>

#include "Bridge/INTP001A.h"

using namespace Sunny::Infrastructure;

// =============================================================================
// CommandBuffer: basic recording
// =============================================================================

TEST_CASE("INTP001A: CommandBuffer records send calls", "[bridge][transport]") {
    CommandBuffer buf;
    CHECK(buf.is_connected());
    CHECK(buf.size() == 0);

    auto req = LomProtocol::set_property(LomPaths::song(), "tempo", 120.0);
    auto resp = buf.send(req);

    CHECK(resp.success);
    REQUIRE(buf.size() == 1);
    CHECK(buf.entries()[0].request.type == LomRequestType::SetProperty);
    CHECK(buf.entries()[0].request.property_or_method == "tempo");
}

TEST_CASE("INTP001A: CommandBuffer records send_notes calls", "[bridge][transport]") {
    CommandBuffer buf;

    std::vector<LomNoteData> notes = {
        {60, 0.0, 1.0, 100, false},
        {64, 1.0, 0.5, 80, false}
    };

    auto resp = buf.send_notes(LomPaths::clip(0, 0), notes);
    CHECK(resp.success);
    REQUIRE(buf.size() == 1);
    CHECK(buf.entries()[0].request.property_or_method == "add_notes");
    CHECK(buf.entries()[0].notes.size() == 2);
    CHECK(buf.entries()[0].notes[0].pitch == 60);
    CHECK(buf.entries()[0].notes[1].pitch == 64);
}

TEST_CASE("INTP001A: CommandBuffer clear resets state", "[bridge][transport]") {
    CommandBuffer buf;
    buf.send(LomProtocol::get_property(LomPaths::song(), "tempo"));
    buf.send(LomProtocol::get_property(LomPaths::song(), "tempo"));
    REQUIRE(buf.size() == 2);

    buf.clear();
    CHECK(buf.size() == 0);
    CHECK(buf.entries().empty());
}

// =============================================================================
// CommandBuffer: query methods
// =============================================================================

TEST_CASE("INTP001A: CommandBuffer find_by_type", "[bridge][transport]") {
    CommandBuffer buf;
    buf.send(LomProtocol::set_property(LomPaths::song(), "tempo", 120.0));
    buf.send(LomProtocol::get_property(LomPaths::track(0), "volume"));
    buf.send(LomProtocol::set_property(LomPaths::track(0), "name", std::string("Piano")));

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    CHECK(sets.size() == 2);

    auto gets = buf.find_by_type(LomRequestType::GetProperty);
    CHECK(gets.size() == 1);
}

TEST_CASE("INTP001A: CommandBuffer find_by_path_prefix", "[bridge][transport]") {
    CommandBuffer buf;
    buf.send(LomProtocol::set_property(LomPaths::song(), "tempo", 120.0));
    buf.send(LomProtocol::set_property(LomPaths::track(0), "volume", 0.8));
    buf.send(LomProtocol::set_property(LomPaths::track(1), "volume", 0.6));

    auto song_cmds = buf.find_by_path_prefix("song");
    CHECK(song_cmds.size() == 3);  // all paths start with "song"

    auto track_cmds = buf.find_by_path_prefix("song/tracks");
    CHECK(track_cmds.size() == 2);
}

TEST_CASE("INTP001A: CommandBuffer count_type", "[bridge][transport]") {
    CommandBuffer buf;
    buf.send(LomProtocol::call_method(LomPaths::song(), "create_midi_track", {0}));
    buf.send(LomProtocol::call_method(LomPaths::song(), "create_midi_track", {1}));
    buf.send(LomProtocol::set_property(LomPaths::song(), "tempo", 120.0));

    CHECK(buf.count_type(LomRequestType::CallMethod) == 2);
    CHECK(buf.count_type(LomRequestType::SetProperty) == 1);
    CHECK(buf.count_type(LomRequestType::GetProperty) == 0);
}

// =============================================================================
// TcpTransport: state management
// =============================================================================

TEST_CASE("INTP001A: TcpTransport starts disconnected", "[bridge][transport]") {
    TcpTransport tcp;
    CHECK_FALSE(tcp.is_connected());
    CHECK(tcp.state() == ConnectionState::Disconnected);
}

TEST_CASE("INTP001A: TcpTransport send fails when disconnected", "[bridge][transport]") {
    TcpTransport tcp;
    auto resp = tcp.send(LomProtocol::get_property(LomPaths::song(), "tempo"));
    CHECK_FALSE(resp.success);
    REQUIRE(resp.error.has_value());
    CHECK(*resp.error == "not connected");
}

TEST_CASE("INTP001A: TcpTransport send_notes fails when disconnected", "[bridge][transport]") {
    TcpTransport tcp;
    auto resp = tcp.send_notes(LomPaths::clip(0, 0), {});
    CHECK_FALSE(resp.success);
}

TEST_CASE("INTP001A: TcpTransport connect fails without server", "[bridge][transport]") {
    TcpTransport tcp;
    CHECK_FALSE(tcp.connect());
    CHECK(tcp.state() == ConnectionState::Error);
}

TEST_CASE("INTP001A: TcpTransport disconnect transitions to Disconnected", "[bridge][transport]") {
    TcpTransport tcp;
    tcp.connect();  // moves to Error
    tcp.disconnect();
    CHECK(tcp.state() == ConnectionState::Disconnected);
}

TEST_CASE("INTP001A: TcpTransport state change callback", "[bridge][transport]") {
    TcpTransport tcp;
    std::vector<ConnectionState> observed;
    tcp.on_state_change([&](ConnectionState s) { observed.push_back(s); });

    tcp.connect();
    tcp.disconnect();

    REQUIRE(observed.size() == 2);
    CHECK(observed[0] == ConnectionState::Error);
    CHECK(observed[1] == ConnectionState::Disconnected);
}
