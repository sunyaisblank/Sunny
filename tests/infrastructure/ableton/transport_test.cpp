/**
 * @file transport_test.cpp
 * @brief Unit tests for LOM Transport
 *
 *
 * Coverage: CommandBuffer recording, query methods, TcpTransport state,
 *           TcpTransport connection lifecycle and host resolution against a
 *           scripted loopback peer
 */

#include <arpa/inet.h>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <poll.h>
#include <string>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace sunny::infrastructure;
using namespace std::chrono_literals;

namespace {

std::optional<std::string> read_frame(int client) {
    const auto read_exact = [client](char* data, std::size_t size) {
        std::size_t received = 0;
        while (received < size) {
            const auto count = ::recv(client, data + received, size - received, 0);
            if (count <= 0) return false;
            received += static_cast<std::size_t>(count);
        }
        return true;
    };
    std::uint32_t network_length = 0;
    if (!read_exact(reinterpret_cast<char*>(&network_length), sizeof(network_length)))
        return std::nullopt;
    std::string payload(ntohl(network_length), '\0');
    if (!read_exact(payload.data(), payload.size())) return std::nullopt;
    return payload;
}

void write_bytes(int client, const std::string& bytes) {
    // MSG_NOSIGNAL: a scripted late write to a client that already left must
    // not kill the test process.
    (void)::send(client, bytes.data(), bytes.size(), MSG_NOSIGNAL);
}

std::string frame(const std::string& payload) {
    const std::uint32_t network_length = htonl(static_cast<std::uint32_t>(payload.size()));
    return std::string(reinterpret_cast<const char*>(&network_length), sizeof(network_length)) +
           payload;
}

std::string response_with_value(const std::string& value_json) {
    return R"({"bridge_protocol_version":43,"success":true,"value":)" + value_json + "}";
}

/// Loopback peer that runs one scripted session per accepted connection, in
/// order, closing each connection when its session returns. Sequential
/// sessions model the Remote Script, which serves one client at a time.
class ScriptedPeer {
  public:
    using Session = std::function<void(int client)>;

    explicit ScriptedPeer(std::vector<Session> sessions, int family = AF_INET)
        : sessions_(std::move(sessions)) {
        listener_ = ::socket(family, SOCK_STREAM, 0);
        REQUIRE(listener_ >= 0);
        int reuse = 1;
        ::setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        if (family == AF_INET6) {
            sockaddr_in6 address{};
            address.sin6_family = AF_INET6;
            address.sin6_addr = in6addr_loopback;
            bound_ = ::bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
            socklen_t size = sizeof(address);
            ::getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &size);
            port_ = ntohs(address.sin6_port);
        } else {
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            bound_ = ::bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
            socklen_t size = sizeof(address);
            ::getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &size);
            port_ = ntohs(address.sin_port);
        }
        if (!bound_ || ::listen(listener_, 4) != 0) {
            bound_ = false;
            return;
        }
        thread_ = std::thread([this] { serve(); });
    }

    ~ScriptedPeer() {
        stop_ = true;
        if (thread_.joinable()) thread_.join();
        stop_listening();
    }

    ScriptedPeer(const ScriptedPeer&) = delete;
    ScriptedPeer& operator=(const ScriptedPeer&) = delete;
    ScriptedPeer(ScriptedPeer&&) = delete;
    ScriptedPeer& operator=(ScriptedPeer&&) = delete;

    [[nodiscard]] bool bound() const { return bound_; }
    [[nodiscard]] std::uint16_t port() const { return port_; }

    /// Refuse further connections, as a Remote Script that has unloaded.
    void stop_listening() {
        std::lock_guard lock(listener_mutex_);
        if (listener_ >= 0) ::close(listener_);
        listener_ = -1;
    }

    /// Block until `count` sessions have returned and their sockets are closed.
    void wait_for_closed_sessions(std::size_t count) {
        std::unique_lock lock(mutex_);
        REQUIRE(closed_changed_.wait_for(lock, 5s, [&] { return closed_ >= count; }));
        // Loopback delivers the FIN before close returns; this margin only
        // absorbs scheduler noise on a loaded host.
        std::this_thread::sleep_for(20ms);
    }

  private:
    void serve() {
        for (auto& session : sessions_) {
            int client = -1;
            while (!stop_ && client < 0) {
                int listener = -1;
                {
                    std::lock_guard lock(listener_mutex_);
                    listener = listener_;
                }
                if (listener < 0) return;
                pollfd descriptor{listener, POLLIN, 0};
                if (::poll(&descriptor, 1, 20) > 0) client = ::accept(listener, nullptr, nullptr);
            }
            if (client < 0) return;
            session(client);
            ::close(client);
            {
                std::lock_guard lock(mutex_);
                ++closed_;
            }
            closed_changed_.notify_all();
        }
    }

    std::vector<Session> sessions_;
    int listener_{-1};
    bool bound_{false};
    std::uint16_t port_{0};
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::mutex listener_mutex_;
    std::mutex mutex_;
    std::condition_variable closed_changed_;
    std::size_t closed_{0};
};

ScriptedPeer::Session answer_each_request_with(std::string value_json) {
    return [value = std::move(value_json)](int client) {
        while (read_frame(client))
            write_bytes(client, frame(response_with_value(value)));
    };
}

ScriptedPeer::Session answer_one_request_with(std::string value_json) {
    return [value = std::move(value_json)](int client) {
        if (read_frame(client)) write_bytes(client, frame(response_with_value(value)));
    };
}

TcpConfig loopback_config(std::uint16_t port, std::string host = "127.0.0.1") {
    TcpConfig config;
    config.host = std::move(host);
    config.port = port;
    config.connect_timeout = 500ms;
    config.response_timeout = 2s;
    return config;
}

LomRequest tempo_request() {
    return LomProtocol::get_property(LomPaths::song(), "tempo");
}

} // namespace

// =============================================================================
// CommandBuffer: basic recording
// =============================================================================

TEST_CASE("CommandBuffer records send calls", "[bridge][transport]") {
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

TEST_CASE("CommandBuffer records send_notes calls", "[bridge][transport]") {
    CommandBuffer buf;

    std::vector<LomNoteData> notes = {{60, 0.0, 1.0, 100, false}, {64, 1.0, 0.5, 80, false}};

    auto resp = buf.send_notes(LomPaths::clip(0, 0), notes);
    CHECK(resp.success);
    REQUIRE(buf.size() == 1);
    CHECK(buf.entries()[0].request.property_or_method == "add_new_notes");
    CHECK(buf.entries()[0].notes.size() == 2);
    CHECK(buf.entries()[0].notes[0].pitch == 60);
    CHECK(buf.entries()[0].notes[1].pitch == 64);
}

TEST_CASE("transports decline noncanonical paths without recording or sending",
          "[bridge][transport][path]") {
    const auto path = LomPath::parse("/song/tracks/0");
    REQUIRE_FALSE(path.is_canonical());

    CommandBuffer buffer;
    const auto recorded = buffer.send(LomProtocol::get_property(path, "name"));
    CHECK_FALSE(recorded.success);
    CHECK(recorded.delivery == LomDeliveryState::NotSent);
    CHECK(buffer.entries().empty());

    const auto notes = buffer.send_notes(path, {});
    CHECK_FALSE(notes.success);
    CHECK(notes.delivery == LomDeliveryState::NotSent);
    CHECK(buffer.entries().empty());

    TcpTransport tcp;
    const auto network = tcp.send(LomProtocol::get_property(path, "name"));
    CHECK_FALSE(network.success);
    CHECK(network.delivery == LomDeliveryState::NotSent);
    REQUIRE(network.error.has_value());
    CHECK(*network.error == "request outside Sunny bridge protocol v43");
}

TEST_CASE("transports decline requests outside the peer algebra before recording",
          "[bridge][transport][trust-boundary]") {
    CommandBuffer buffer;
    const auto response = buffer.send(LomProtocol::get_property(LomPaths::track(0), "name"));
    CHECK_FALSE(response.success);
    CHECK(response.delivery == LomDeliveryState::NotSent);
    CHECK(buffer.entries().empty());

    const auto notes = buffer.send_notes(LomPaths::clip(0, 0), {});
    CHECK_FALSE(notes.success);
    CHECK(notes.delivery == LomDeliveryState::NotSent);
    CHECK(buffer.entries().empty());
}

TEST_CASE("CommandBuffer clear resets state", "[bridge][transport]") {
    CommandBuffer buf;
    buf.send(LomProtocol::get_property(LomPaths::song(), "tempo"));
    buf.send(LomProtocol::get_property(LomPaths::song(), "tempo"));
    REQUIRE(buf.size() == 2);

    buf.clear();
    CHECK(buf.size() == 0);
    CHECK(buf.entries().empty());
}

TEST_CASE("CommandBuffer models inserted device counts and clears them",
          "[bridge][transport][target-state]") {
    CommandBuffer buffer;
    const auto track = LomPaths::track(2);
    buffer.set_device_count(track, 4);
    buffer.send(LomProtocol::call_method(track, "insert_device", {std::string("EQ Eight")}));

    auto count = buffer.device_count(track);
    REQUIRE(count.has_value());
    REQUIRE(count->has_value());
    CHECK(**count == 5);

    buffer.clear();
    count = buffer.device_count(track);
    REQUIRE(count.has_value());
    REQUIRE(count->has_value());
    CHECK(**count == 0);
}

// =============================================================================
// CommandBuffer: query methods
// =============================================================================

TEST_CASE("CommandBuffer find_by_type", "[bridge][transport]") {
    CommandBuffer buf;
    buf.send(LomProtocol::set_property(LomPaths::song(), "tempo", 120.0));
    buf.send(LomProtocol::get_property(LomPaths::song(), "is_playing"));
    buf.send(LomProtocol::set_property(LomPaths::track(0), "name", std::string("Piano")));

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    CHECK(sets.size() == 2);

    auto gets = buf.find_by_type(LomRequestType::GetProperty);
    CHECK(gets.size() == 1);
}

TEST_CASE("CommandBuffer find_by_path_prefix", "[bridge][transport]") {
    CommandBuffer buf;
    buf.send(LomProtocol::set_property(LomPaths::song(), "tempo", 120.0));
    buf.send(LomProtocol::set_property(LomPaths::track(0), "name", std::string("One")));
    buf.send(LomProtocol::set_property(LomPaths::track(1), "name", std::string("Two")));

    auto song_cmds = buf.find_by_path_prefix("song");
    CHECK(song_cmds.size() == 3); // all paths start with "song"

    auto track_cmds = buf.find_by_path_prefix("song/tracks");
    CHECK(track_cmds.size() == 2);
}

TEST_CASE("CommandBuffer count_type", "[bridge][transport]") {
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

TEST_CASE("TcpTransport starts disconnected", "[bridge][transport]") {
    TcpTransport tcp;
    CHECK_FALSE(tcp.is_connected());
    CHECK(tcp.state() == ConnectionState::Disconnected);
}

TEST_CASE("TcpTransport send fails when disconnected", "[bridge][transport]") {
    TcpTransport tcp;
    auto resp = tcp.send(LomProtocol::get_property(LomPaths::song(), "tempo"));
    CHECK_FALSE(resp.success);
    REQUIRE(resp.error.has_value());
    CHECK(*resp.error == "not connected");
}

TEST_CASE("TcpTransport send_notes fails when disconnected", "[bridge][transport]") {
    TcpTransport tcp;
    auto resp = tcp.send_notes(LomPaths::clip(0, 0), {});
    CHECK_FALSE(resp.success);
}

TEST_CASE("TcpTransport connect fails without server", "[bridge][transport]") {
    TcpTransport tcp;
    CHECK_FALSE(tcp.connect());
    CHECK(tcp.state() == ConnectionState::Error);
}

TEST_CASE("TcpTransport disconnect transitions to Disconnected", "[bridge][transport]") {
    TcpTransport tcp;
    CHECK_FALSE(tcp.connect()); // moves to Error
    tcp.disconnect();
    CHECK(tcp.state() == ConnectionState::Disconnected);
}

TEST_CASE("TcpTransport state change callback", "[bridge][transport]") {
    TcpTransport tcp;
    std::vector<ConnectionState> observed;
    tcp.on_state_change([&](ConnectionState s) { observed.push_back(s); });

    CHECK_FALSE(tcp.connect());
    tcp.disconnect();

    REQUIRE(observed.size() == 3);
    CHECK(observed[0] == ConnectionState::Connecting);
    CHECK(observed[1] == ConnectionState::Error);
    CHECK(observed[2] == ConnectionState::Disconnected);
}

// =============================================================================
// TcpTransport: connection lifecycle against a scripted peer
// =============================================================================

TEST_CASE("an idle connection closed by the peer is replaced before the next request is sent",
          "[bridge][transport][loopback][lifecycle]") {
    ScriptedPeer peer({answer_one_request_with("1"), answer_each_request_with("2")});
    TcpTransport transport(loopback_config(peer.port()));
    REQUIRE(transport.connect());

    const auto first = transport.send(tempo_request());
    REQUIRE(first.success);
    CHECK(std::get<int>(*first.value) == 1);

    peer.wait_for_closed_sessions(1);
    const auto second = transport.send(tempo_request());
    REQUIRE(second.success);
    CHECK(second.delivery == LomDeliveryState::ResponseReceived);
    CHECK(std::get<int>(*second.value) == 2);
    CHECK(transport.is_connected());
}

TEST_CASE("ensure_connected replaces an idle connection closed by the peer",
          "[bridge][transport][loopback][lifecycle]") {
    ScriptedPeer peer({[](int) {}, answer_each_request_with("2")});
    TcpTransport transport(loopback_config(peer.port()));
    REQUIRE(transport.connect());

    peer.wait_for_closed_sessions(1);
    REQUIRE(transport.ensure_connected());
    const auto response = transport.send(tempo_request());
    REQUIRE(response.success);
    CHECK(std::get<int>(*response.value) == 2);
}

TEST_CASE("a request after the peer vanished while idle is declined as not sent",
          "[bridge][transport][loopback][lifecycle]") {
    ScriptedPeer peer({answer_one_request_with("1")});
    TcpTransport transport(loopback_config(peer.port()));
    REQUIRE(transport.connect());
    REQUIRE(transport.send(tempo_request()).success);

    peer.wait_for_closed_sessions(1);
    peer.stop_listening();
    const auto declined = transport.send(tempo_request());
    CHECK_FALSE(declined.success);
    CHECK(declined.delivery == LomDeliveryState::NotSent);
    CHECK_FALSE(transport.is_connected());
}

TEST_CASE("a response later than the deadline is indeterminate and never answers a later request",
          "[bridge][transport][loopback][lifecycle][indeterminate]") {
    ScriptedPeer peer({[](int client) {
                           if (!read_frame(client)) return;
                           std::this_thread::sleep_for(400ms);
                           write_bytes(client, frame(response_with_value(R"("late")")));
                       },
                       answer_each_request_with(R"("fresh")")});
    auto config = loopback_config(peer.port());
    config.response_timeout = 100ms;
    TcpTransport transport(config);
    REQUIRE(transport.connect());

    const auto abandoned = transport.send(tempo_request());
    CHECK_FALSE(abandoned.success);
    CHECK(abandoned.delivery == LomDeliveryState::SentWithoutValidResponse);
    REQUIRE(abandoned.error.has_value());
    CHECK(abandoned.error->starts_with("delivery indeterminate:"));
    CHECK_FALSE(transport.is_connected());

    // The single-client peer finishes the abandoned request before it accepts
    // the replacement connection, as the Remote Script does.
    peer.wait_for_closed_sessions(1);
    REQUIRE(transport.ensure_connected());
    const auto fresh = transport.send(tempo_request());
    REQUIRE(fresh.success);
    CHECK(std::get<std::string>(*fresh.value) == "fresh");
}

TEST_CASE("the response deadline bounds the whole response rather than each receive",
          "[bridge][transport][loopback][lifecycle][indeterminate]") {
    ScriptedPeer peer({[](int client) {
        if (!read_frame(client)) return;
        const auto bytes = frame(response_with_value("1"));
        for (const char byte : bytes) {
            write_bytes(client, std::string(1, byte));
            std::this_thread::sleep_for(10ms);
        }
    }});
    auto config = loopback_config(peer.port());
    config.response_timeout = 150ms;
    TcpTransport transport(config);
    REQUIRE(transport.connect());

    const auto dripped = transport.send(tempo_request());
    CHECK_FALSE(dripped.success);
    CHECK(dripped.delivery == LomDeliveryState::SentWithoutValidResponse);
    CHECK_FALSE(transport.is_connected());
}

// =============================================================================
// TcpTransport: host resolution and connection failure classification
// =============================================================================

TEST_CASE("TcpTransport resolves a host name to the loopback peer",
          "[bridge][transport][loopback][resolution]") {
    ScriptedPeer peer({answer_each_request_with("1")});
    TcpTransport transport(loopback_config(peer.port(), "localhost"));
    REQUIRE(transport.connect());
    CHECK_FALSE(transport.last_connect_failure().has_value());
    CHECK(transport.send(tempo_request()).success);
}

TEST_CASE("TcpTransport connects to an IPv6 loopback literal",
          "[bridge][transport][loopback][resolution]") {
    ScriptedPeer peer({answer_each_request_with("1")}, AF_INET6);
    if (!peer.bound()) SKIP("IPv6 loopback is unavailable on this host");
    TcpTransport transport(loopback_config(peer.port(), "::1"));
    REQUIRE(transport.connect());
    CHECK(transport.send(tempo_request()).success);
}

TEST_CASE("TcpTransport classifies why a connection could not be made",
          "[bridge][transport][resolution]") {
    SECTION("a name that does not resolve") {
        // RFC 6761 reserves .invalid so that it never resolves.
        TcpTransport transport(loopback_config(9001, "sunny-bridge-test.invalid"));
        CHECK_FALSE(transport.connect());
        CHECK(transport.last_connect_failure() == ConnectFailure::HostUnresolved);
    }

    SECTION("a resolved address with nothing listening") {
        TcpTransport transport(loopback_config(1));
        CHECK_FALSE(transport.connect());
        CHECK(transport.last_connect_failure() == ConnectFailure::Refused);
    }

    SECTION("a configuration that cannot bound a request") {
        auto config = loopback_config(9001);
        config.response_timeout = 0ms;
        TcpTransport transport(config);
        CHECK_FALSE(transport.connect());
        CHECK(transport.last_connect_failure() == ConnectFailure::InvalidConfiguration);
    }

    SECTION("each failure has a distinct description") {
        CHECK(describe(ConnectFailure::HostUnresolved) != describe(ConnectFailure::Refused));
        CHECK(describe(ConnectFailure::Refused) != describe(ConnectFailure::TimedOut));
    }
}
