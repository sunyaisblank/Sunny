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
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <functional>
#include <mutex>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <poll.h>
#include <string>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sunny/infrastructure/request_control.hpp>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace sunny::infrastructure;
using namespace std::chrono_literals;

namespace {

std::optional<std::string>
read_frame(int client,
           std::optional<std::chrono::steady_clock::time_point> deadline = std::nullopt) {
    const auto read_exact = [client, deadline](char* data, std::size_t size) {
        std::size_t received = 0;
        while (received < size) {
            if (deadline) {
                if (std::chrono::steady_clock::now() >= *deadline) return false;
                pollfd descriptor{client, POLLIN, 0};
                const auto ready = ::poll(&descriptor, 1, 10);
                if (ready < 0 && errno == EINTR) continue;
                if (ready < 0) return false;
                if (ready == 0) continue;
            }
            const auto count =
                ::recv(client, data + received, size - received, deadline ? MSG_DONTWAIT : 0);
            if (deadline && count < 0 &&
                (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
                continue;
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
    return R"({"bridge_protocol_version":47,"success":true,"value":)" + value_json + "}";
}

bool answer_identity_request(int client, const std::string& request) {
    const auto value = nlohmann::json::parse(request);
    if (value.value("name", "") != "sunny_get_target_profile") return false;
    const auto profile = target_profile_to_json(modeled_target_profile({12, 4, 0, "12.4.0"}));
    write_bytes(client, frame(response_with_value(profile.dump())));
    return true;
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
        while (const auto request = read_frame(client)) {
            if (!answer_identity_request(client, *request))
                write_bytes(client, frame(response_with_value(value)));
        }
    };
}

ScriptedPeer::Session answer_one_request_with(std::string value_json) {
    return [value = std::move(value_json)](int client) {
        while (const auto request = read_frame(client)) {
            if (answer_identity_request(client, *request)) continue;
            write_bytes(client, frame(response_with_value(value)));
            return;
        }
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

LomRequest
prepared_mutation(nlohmann::json command = {
                      {"type", "set"}, {"path", "song"}, {"name", "tempo"}, {"args", {140.0}}}) {
    const nlohmann::json intent{{"schema_version", 1},
                                {"bridge_instance", std::string(32, 'b')},
                                {"document_token", std::string(32, 'd')},
                                {"scope_id", std::string(32, 'a')},
                                {"workflow_id", std::string(32, 'c')},
                                {"operation_id", std::string(32, 'e')},
                                {"ordinal", 1},
                                {"graph_revision", 0},
                                {"command", std::move(command)}};
    return LomProtocol::call_method(LomPaths::song(), "sunny_legacy_prepare", {intent});
}

std::shared_ptr<RequestControl> admitted_control() {
    return std::make_shared<RequestControl>(
        "number:transport",
        "tools/call",
        std::chrono::steady_clock::now() + MCP_REQUEST_TIMEOUT,
        NativeOrigin{std::string(32, 'b'), std::string(32, 'd')},
        "legacy_ableton_request");
}

/// A real exec'ed resolver fault peer; no substitute for transport process ownership.
class ResolverPeer {
  public:
    explicit ResolverPeer(std::string body) {
        char pattern[] = "/tmp/sunny-resolver-test-XXXXXX";
        const auto descriptor = ::mkstemp(pattern);
        REQUIRE(descriptor >= 0);
        ::close(descriptor);
        path_ = pattern;
        pid_path_ = path_ + ".pid";
        std::ofstream script(path_);
        script << "#!/usr/bin/env python3\nimport os,time,sys\n"
               << "with open(" << nlohmann::json(pid_path_).dump()
               << ",'w') as output: output.write(str(os.getpid()))\n"
               << body << '\n';
        script.close();
        REQUIRE(::chmod(path_.c_str(), 0700) == 0);
    }
    ~ResolverPeer() {
        ::unlink(path_.c_str());
        ::unlink(pid_path_.c_str());
    }
    const std::string& path() const { return path_; }
    void check_reaped() const {
        std::ifstream input(pid_path_);
        int identifier = 0;
        input >> identifier;
        REQUIRE(identifier > 0);
        const auto alive = ::kill(identifier, 0);
        const auto absent_errno = errno;
        CHECK(alive == -1);
        CHECK(absent_errno == ESRCH);
        int status = 0;
        const auto waited = ::waitpid(identifier, &status, WNOHANG);
        const auto reaped_errno = errno;
        CHECK(waited == -1);
        CHECK(reaped_errno == ECHILD);
    }

  private:
    std::string path_, pid_path_;
};

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
    CHECK(*network.error == "request outside Sunny bridge protocol v47");
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
                           const auto identity = read_frame(client);
                           if (!identity || !answer_identity_request(client, *identity)) return;
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
        const auto identity = read_frame(client);
        if (!identity || !answer_identity_request(client, *identity)) return;
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
    auto config = loopback_config(peer.port(), "::1");
    config.resolver_executable = "/missing/sunny-resolver";
    TcpTransport transport(config);
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

TEST_CASE(
    "Bridge source mismatch declines direct mutation and notes while diagnostics remain readable",
    "[bridge][transport][loopback][source-identity]") {
    for (const auto* problem : {"missing", "wrong_type", "malformed", "different", "contract"}) {
        INFO(problem);
        auto profile = target_profile_to_json(modeled_target_profile({12, 4, 0, "12.4.0"}));
        if (std::string_view(problem) == "missing")
            profile["adapter"].erase("source_sha256");
        else if (std::string_view(problem) == "wrong_type")
            profile["adapter"]["source_sha256"] = 46;
        else if (std::string_view(problem) == "malformed")
            profile["adapter"]["source_sha256"] = "unknown";
        else if (std::string_view(problem) == "different")
            profile["adapter"]["source_sha256"] = std::string(64, '0');
        else
            profile["adapter"]["contract"] = "documented_public";
        std::atomic<unsigned> identity_reads = 0, ordinary_requests = 0, diagnostic_reads = 0;
        ScriptedPeer peer({[&](int client) {
            while (const auto request = read_frame(client)) {
                const auto wire = nlohmann::json::parse(*request);
                const auto name = wire.at("name").get<std::string>();
                if (name == "sunny_get_target_profile") {
                    ++identity_reads;
                    write_bytes(client, frame(response_with_value(profile.dump())));
                } else if (name == "sunny_get_remote_log") {
                    ++diagnostic_reads;
                    write_bytes(client, frame(response_with_value(R"({"message":"available"})")));
                } else {
                    ++ordinary_requests;
                    write_bytes(client, frame(response_with_value("120.0")));
                }
            }
        }});
        TcpTransport transport(loopback_config(peer.port()));
        REQUIRE(transport.connect());
        RequestControlScope origin_scope(admitted_control());
        const auto mutation = transport.send(prepared_mutation());
        CHECK_FALSE(mutation.success);
        CHECK(mutation.delivery == LomDeliveryState::NotSent);
        REQUIRE(mutation.error);
        CHECK(mutation.error->find("same server image") != std::string::npos);
        if (std::string_view(problem) == "different") {
            CHECK(mutation.error->find(SUNNY_BRIDGE_SOURCE_SHA256) != std::string::npos);
            CHECK(mutation.error->find(std::string(64, '0')) != std::string::npos);
        }
        const auto notes = transport.send_notes(LomPaths::clip(0, 0), {});
        CHECK_FALSE(notes.success);
        CHECK(notes.delivery == LomDeliveryState::NotSent);
        CHECK(ordinary_requests == 0);
        CHECK(identity_reads == 1); // Failed compatibility is retained on this socket.
        CHECK(transport.bridge_identity_error().has_value());
        const auto log =
            transport.send(LomProtocol::call_method(LomPaths::song(), "sunny_get_remote_log", {0}));
        CHECK(log.success);
        CHECK(diagnostic_reads == 1);
        CHECK_FALSE(transport.target_profile()); // Diagnostic remains readable, admission fails.
        CHECK(identity_reads == 2);
        CHECK(ordinary_requests == 0);
        transport.disconnect();
        peer.wait_for_closed_sessions(1);
    }
}

TEST_CASE("Matching source handshake is cached per socket and refreshed after reconnect",
          "[bridge][transport][loopback][source-identity]") {
    std::atomic<unsigned> identity_reads = 0, ordinary_requests = 0;
    const auto matched = target_profile_to_json(modeled_target_profile({12, 4, 0, "12.4.0"}));
    auto mismatched = matched;
    mismatched["adapter"]["source_sha256"] = std::string(64, '0');
    const auto session = [&](const nlohmann::json& profile, unsigned request_limit) {
        return [&, profile, request_limit](int client) {
            unsigned ordinary_in_session = 0;
            while (const auto request = read_frame(client)) {
                const auto wire = nlohmann::json::parse(*request);
                if (wire.at("name") == "sunny_get_target_profile") {
                    ++identity_reads;
                    write_bytes(client, frame(response_with_value(profile.dump())));
                } else {
                    ++ordinary_requests;
                    ++ordinary_in_session;
                    write_bytes(client, frame(response_with_value("120.0")));
                    if (ordinary_in_session == request_limit) return;
                }
            }
        };
    };
    ScriptedPeer peer({session(matched, 2), session(mismatched, 1)});
    TcpTransport transport(loopback_config(peer.port()));
    REQUIRE(transport.connect());
    RequestControlScope origin_scope(admitted_control());
    REQUIRE(transport.send(tempo_request()).success);
    REQUIRE(transport.send(tempo_request()).success);
    CHECK(identity_reads == 1);
    CHECK(ordinary_requests == 2);
    peer.wait_for_closed_sessions(1);
    const auto declined = transport.send(prepared_mutation());
    CHECK_FALSE(declined.success);
    CHECK(declined.delivery == LomDeliveryState::NotSent);
    CHECK(identity_reads == 2);
    CHECK(ordinary_requests == 2);
    transport.disconnect();
    peer.wait_for_closed_sessions(2);
}

TEST_CASE("An uncertain read-only identity handshake never sends the caller mutation",
          "[bridge][transport][loopback][source-identity]") {
    std::atomic<unsigned> frames = 0;
    ScriptedPeer peer({[&](int client) {
        if (const auto request = read_frame(client)) {
            ++frames;
            const auto wire = nlohmann::json::parse(*request);
            if (wire.at("name") != "sunny_get_target_profile") return;
            std::this_thread::sleep_for(200ms);
        }
    }});
    auto config = loopback_config(peer.port());
    config.response_timeout = 50ms;
    TcpTransport transport(config);
    REQUIRE(transport.connect());
    RequestControlScope origin_scope(admitted_control());
    const auto declined = transport.send(prepared_mutation());
    CHECK_FALSE(declined.success);
    CHECK(declined.delivery == LomDeliveryState::NotSent);
    CHECK(frames == 1);
    CHECK_FALSE(transport.is_connected());
    CHECK(transport.bridge_identity_error().has_value());
    peer.wait_for_closed_sessions(1);
}

TEST_CASE("Numeric endpoints need no hostname resolver runtime",
          "[bridge][transport][loopback][resolution]") {
    ScriptedPeer peer({answer_each_request_with("1")});
    auto config = loopback_config(peer.port());
    config.resolver_executable = "/missing/sunny-resolver";
    TcpTransport transport(config);
    REQUIRE(transport.connect());
    CHECK(transport.send(tempo_request()).success);
}

TEST_CASE("Docker hostname is passed literally to the isolated owned resolver",
          "[bridge][transport][loopback][resolution]") {
    ScriptedPeer peer({answer_each_request_with("1")});
    ResolverPeer resolver("assert sys.argv[-2]=='host.docker.internal'\n"
                          "assert '-I' in sys.argv and '-S' in sys.argv\n"
                          "print('[[2,\"127.0.0.1\",0]]')");
    auto config = loopback_config(peer.port(), "host.docker.internal");
    config.resolver_executable = resolver.path();
    TcpTransport transport(config);
    REQUIRE(transport.connect());
    CHECK(transport.send(tempo_request()).success);
    resolver.check_reaped();
}

TEST_CASE("Resolver failures are finite and every owned resolver PID is reaped",
          "[bridge][transport][resolution][deadline]") {
    SECTION("missing helper differs from unresolved name") {
        auto config = loopback_config(9001, "localhost");
        config.resolver_executable = "/missing/sunny-resolver";
        TcpTransport transport(config);
        CHECK_FALSE(transport.connect());
        CHECK(transport.last_connect_failure() == ConnectFailure::ResolverUnavailable);
    }
    SECTION("a stalled actual resolver shares the connection budget") {
        ResolverPeer resolver("time.sleep(30)");
        auto config = loopback_config(9001, "sunny-resolver-test.invalid");
        config.resolver_executable = resolver.path();
        config.connect_timeout = 60ms;
        TcpTransport transport(config);
        const auto started = std::chrono::steady_clock::now();
        CHECK_FALSE(transport.connect());
        CHECK(std::chrono::steady_clock::now() - started < 250ms);
        CHECK(transport.last_connect_failure() == ConnectFailure::TimedOut);
        resolver.check_reaped();
    }
    SECTION("nonzero runtime failure cannot become unresolved-name evidence") {
        ResolverPeer resolver("sys.exit(3)");
        auto config = loopback_config(9001, "localhost");
        config.resolver_executable = resolver.path();
        TcpTransport transport(config);
        CHECK_FALSE(transport.connect());
        CHECK(transport.last_connect_failure() == ConnectFailure::ResolverUnavailable);
        resolver.check_reaped();
    }
    SECTION("a resolver's definite DNS rejection remains distinct") {
        ResolverPeer resolver("sys.exit(2)");
        auto config = loopback_config(9001, "localhost");
        config.resolver_executable = resolver.path();
        TcpTransport transport(config);
        CHECK_FALSE(transport.connect());
        CHECK(transport.last_connect_failure() == ConnectFailure::HostUnresolved);
        resolver.check_reaped();
    }
    SECTION("malformed, wrong-family and oversized outputs decline") {
        for (const auto* body : {"print('not JSON')",
                                 "print('[[2,\"127.0.0.1\",true]]')",
                                 "print('[[999,\"127.0.0.1\",0]]')",
                                 "print('x'*17000)"}) {
            ResolverPeer resolver(body);
            auto config = loopback_config(9001, "localhost");
            config.resolver_executable = resolver.path();
            TcpTransport transport(config);
            CHECK_FALSE(transport.connect());
            CHECK(transport.last_connect_failure() == ConnectFailure::ResolverUnavailable);
            resolver.check_reaped();
        }
    }
}

TEST_CASE("A stalled large request expires during send and is never replayed",
          "[bridge][transport][loopback][deadline]") {
    std::atomic<unsigned> full_requests = 0;
    ScriptedPeer peer({[&](int client) {
        int window = 1024;
        ::setsockopt(client, SOL_SOCKET, SO_RCVBUF, &window, sizeof(window));
        const auto identity = read_frame(client);
        if (!identity || !answer_identity_request(client, *identity)) return;
        std::this_thread::sleep_for(300ms);
        window = 1024 * 1024;
        ::setsockopt(client, SOL_SOCKET, SO_RCVBUF, &window, sizeof(window));
        // A tiny receive window may continue delivering a queued fragment
        // long after the sender closes. Bound the peer's entire drain rather
        // than resetting SO_RCVTIMEO every time a few more bytes arrive.
        if (read_frame(client, std::chrono::steady_clock::now() + 1s)) ++full_requests;
    }});
    auto config = loopback_config(peer.port());
    config.response_timeout = 80ms;
    TcpTransport transport(config);
    REQUIRE(transport.connect());
    RequestControlScope origin_scope(admitted_control());
    const std::vector<LomNoteData> notes(50000, LomNoteData{60, 0.0, 1.0, 100, false});
    REQUIRE(LomProtocol::validate_notes(LomPaths::clip(0, 0), notes));
    const auto payload =
        LomProtocol::serialize_request(LomProtocol::add_new_notes(LomPaths::clip(0, 0), notes));
    REQUIRE(payload.size() > 4U * 1024U * 1024U);
    REQUIRE(payload.size() < SUNNY_BRIDGE_MAX_WIRE_PAYLOAD);
    auto command = nlohmann::json::parse(payload);
    command.erase("bridge_protocol_version");
    const auto request = prepared_mutation(std::move(command));
    REQUIRE(LomProtocol::validate_request(request));
    const auto started = std::chrono::steady_clock::now();
    const auto response = transport.send(request);
    CHECK_FALSE(response.success);
    CHECK(response.delivery == LomDeliveryState::NotSent);
    REQUIRE(response.error);
    CHECK(response.error->find("deadline expired before the request frame was complete") !=
          std::string::npos);
    CHECK(std::chrono::steady_clock::now() - started < 1s);
    CHECK_FALSE(transport.is_connected());
    CHECK_FALSE(transport.send(request).success);
    peer.wait_for_closed_sessions(1);
    CHECK(full_requests == 0);
}

TEST_CASE("Queued literal busy admission is NotSent and never reconnects or replays",
          "[bridge][transport][loopback][busy]") {
    std::atomic<unsigned> frames = 0;
    std::atomic<bool> busy_sent = false;
    ScriptedPeer peer({[&](int client) {
        write_bytes(
            client,
            frame(
                R"({"bridge_protocol_version":47,"success":false,"error":"bridge_busy: Sunny accepts one active client; close the existing client and retry"})"));
        busy_sent = true;
        if (read_frame(client)) ++frames;
    }});
    TcpTransport transport(loopback_config(peer.port()));
    REQUIRE(transport.connect());
    RequestControlScope origin_scope(admitted_control());
    const auto ready_deadline = std::chrono::steady_clock::now() + 1s;
    while (!busy_sent && std::chrono::steady_clock::now() < ready_deadline)
        std::this_thread::sleep_for(1ms);
    REQUIRE(busy_sent);
    std::this_thread::sleep_for(10ms); // Ensure the pre-admission frame is readable on loopback.
    const auto declined = transport.send(prepared_mutation());
    CHECK_FALSE(declined.success);
    CHECK(declined.delivery == LomDeliveryState::NotSent);
    CHECK(transport.last_connect_failure() == ConnectFailure::Busy);
    CHECK_FALSE(transport.is_connected());
    REQUIRE(declined.error);
    CHECK(declined.error->find("already has an active client") != std::string::npos);
    peer.wait_for_closed_sessions(1);
    CHECK(frames == 0);
}

TEST_CASE("A literal malformed acknowledgment abandons the socket before any later request",
          "[bridge][transport][loopback][indeterminate]") {
    std::atomic<unsigned> ordinary_requests = 0;
    ScriptedPeer peer(
        {[&](int client) {
             const auto identity = read_frame(client);
             if (!identity || !answer_identity_request(client, *identity)) return;
             if (!read_frame(client)) return;
             ++ordinary_requests;
             write_bytes(client,
                         frame(R"({"bridge_protocol_version":47,"success":"yes","value":120.0})"));
             std::this_thread::sleep_for(100ms);
             write_bytes(client, frame(response_with_value("999.0")));
             if (read_frame(client)) ++ordinary_requests;
         },
         answer_each_request_with("123.0")});
    TcpTransport transport(loopback_config(peer.port()));
    REQUIRE(transport.connect());
    const auto uncertain = transport.send(tempo_request());
    CHECK_FALSE(uncertain.success);
    CHECK(uncertain.delivery == LomDeliveryState::SentWithoutValidResponse);
    CHECK_FALSE(transport.is_connected());
    CHECK(transport.send(tempo_request()).delivery == LomDeliveryState::NotSent);
    peer.wait_for_closed_sessions(1);
    CHECK(ordinary_requests == 1);
    REQUIRE(transport.ensure_connected());
    const auto fresh = transport.send(tempo_request());
    REQUIRE(fresh.success);
    CHECK(std::get<double>(*fresh.value) == 123.0);
}

TEST_CASE("Request revocation before write is definitively NotSent",
          "[bridge][transport][loopback][request-control]") {
    std::atomic<int> frames{0};
    ScriptedPeer peer({[&](int client) {
        while (read_frame(client))
            ++frames;
    }});
    TcpTransport transport(loopback_config(peer.port()));
    REQUIRE(transport.connect());
    auto control = std::make_shared<RequestControl>("number:1", "tools/call");
    control->cancelled = true;
    RequestControlScope scope(control);
    const auto refused =
        transport.send(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0));
    CHECK_FALSE(refused.success);
    CHECK(refused.delivery == LomDeliveryState::NotSent);
    CHECK_FALSE(transport.is_connected());
    peer.wait_for_closed_sessions(1);
    CHECK(frames == 0);
}

TEST_CASE("Physical stateful frames require the immutable admitted epoch before even the handshake",
          "[bridge][transport][loopback][native-origin]") {
    for (const auto* fault : {"no_control", "absent", "malformed", "bridge", "document", "raw"}) {
        INFO(fault);
        std::atomic<unsigned> frames{0};
        ScriptedPeer peer({[&](int client) {
            while (read_frame(client))
                ++frames;
        }});
        TcpTransport transport(loopback_config(peer.port()));
        REQUIRE(transport.connect());
        auto control = admitted_control();
        auto request = prepared_mutation();
        if (std::string_view(fault) == "no_control")
            control.reset();
        else if (std::string_view(fault) == "absent")
            control = std::make_shared<RequestControl>("number:absent", "tools/call");
        else if (std::string_view(fault) == "malformed")
            control = std::make_shared<RequestControl>(
                "number:bad",
                "tools/call",
                std::chrono::steady_clock::now() + 2s,
                NativeOrigin{std::string(32, 'B'), std::string(32, 'd')});
        else if (std::string_view(fault) == "bridge")
            std::get<nlohmann::json>(request.args.front())["bridge_instance"] =
                std::string(32, 'f');
        else if (std::string_view(fault) == "document")
            std::get<nlohmann::json>(request.args.front())["document_token"] = std::string(32, 'f');
        else
            request = LomProtocol::set_property(LomPaths::song(), "tempo", 140.0);
        RequestControlScope scope(control);
        const auto declined = transport.send(request);
        CHECK_FALSE(declined.success);
        CHECK(declined.delivery == LomDeliveryState::NotSent);
        REQUIRE(declined.error);
        CHECK_FALSE(
            transport.bridge_identity_error()); // Fresh metadata cannot grant a missing pin.
        transport.disconnect();
        peer.wait_for_closed_sessions(1);
        CHECK(frames == 0);
    }
}

TEST_CASE("A matching typed frame passes the physical gate once without silently replaying",
          "[bridge][transport][loopback][native-origin]") {
    std::atomic<unsigned> identities{0}, prepared{0};
    ScriptedPeer peer({[&](int client) {
        while (const auto payload = read_frame(client)) {
            if (answer_identity_request(client, *payload)) {
                ++identities;
                continue;
            }
            const auto request = nlohmann::json::parse(*payload);
            if (request.at("name") == "sunny_legacy_prepare" &&
                request.at("args").at(0).at("bridge_instance") == std::string(32, 'b') &&
                request.at("args").at(0).at("document_token") == std::string(32, 'd'))
                ++prepared;
            write_bytes(client, frame(response_with_value("null")));
        }
    }});
    TcpTransport transport(loopback_config(peer.port()));
    REQUIRE(transport.connect());
    RequestControlScope scope(admitted_control());
    const auto response = transport.send(prepared_mutation());
    REQUIRE(response.success);
    CHECK(response.delivery == LomDeliveryState::ResponseReceived);
    transport.disconnect();
    peer.wait_for_closed_sessions(1);
    CHECK(identities == 1);
    CHECK(prepared == 1);
}

TEST_CASE("Retained read-only previews do not drain after EOF",
          "[bridge][transport][loopback][native-origin][request-control]") {
    std::atomic<unsigned> frames{0};
    ScriptedPeer peer({[&](int client) {
        while (read_frame(client))
            ++frames;
    }});
    TcpTransport transport(loopback_config(peer.port()));
    REQUIRE(transport.connect());
    const auto request = LomProtocol::call_method(
        LomPaths::song(),
        "sunny_managed_preview_song_settings",
        {nlohmann::json::parse(
            R"({"document_token":"dddddddddddddddddddddddddddddddd","project_key":"project","binding_key":"part","expected_content_fingerprint":"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee","expected_note_identity_fingerprint":"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff","desired":{"tempo":140.0,"signature_numerator":4,"signature_denominator":4}})")});
    REQUIRE(LomProtocol::is_read_only_request(request));
    REQUIRE(LomProtocol::requires_native_origin(request));
    auto control = admitted_control();
    control->input_closed = true;
    RequestControlScope scope(control);
    const auto declined = transport.send(request);
    CHECK_FALSE(declined.success);
    CHECK(declined.delivery == LomDeliveryState::NotSent);
    CHECK_FALSE(transport.is_connected());
    peer.wait_for_closed_sessions(1);
    CHECK(frames == 0);
}

TEST_CASE("EOF permits read-only native drain but no later native effect",
          "[bridge][transport][loopback][request-control]") {
    std::atomic<int> reads{0}, effects{0};
    ScriptedPeer peer({[&](int client) {
        while (const auto payload = read_frame(client)) {
            if (answer_identity_request(client, *payload)) continue;
            const auto request = nlohmann::json::parse(*payload);
            if (request.at("type") == "get")
                ++reads;
            else
                ++effects;
            write_bytes(client, frame(response_with_value("120.0")));
        }
    }});
    TcpTransport transport(loopback_config(peer.port()));
    REQUIRE(transport.connect());
    auto control = std::make_shared<RequestControl>("number:2", "tools/call");
    control->input_closed = true;
    RequestControlScope scope(control);
    REQUIRE(transport.send(tempo_request()).success);
    const auto refused =
        transport.send(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0));
    CHECK(refused.delivery == LomDeliveryState::NotSent);
    peer.wait_for_closed_sessions(1);
    CHECK(reads == 1);
    CHECK(effects == 0);
}

TEST_CASE("Cancellation after complete native frame is indeterminate and closes the socket",
          "[bridge][transport][loopback][request-control]") {
    auto control = std::make_shared<RequestControl>("number:3", "tools/call");
    std::atomic<int> complete_requests{0};
    ScriptedPeer peer({[&](int client) {
        const auto identity = read_frame(client);
        if (!identity || !answer_identity_request(client, *identity)) return;
        if (!read_frame(client)) return;
        ++complete_requests;
        control->cancelled = true;
        if (read_frame(client)) ++complete_requests;
    }});
    TcpTransport transport(loopback_config(peer.port()));
    REQUIRE(transport.connect());
    RequestControlScope scope(control);
    const auto started = std::chrono::steady_clock::now();
    const auto unknown = transport.send(tempo_request());
    CHECK_FALSE(unknown.success);
    CHECK(unknown.delivery == LomDeliveryState::SentWithoutValidResponse);
    CHECK(std::chrono::steady_clock::now() - started < 300ms);
    CHECK_FALSE(transport.is_connected());
    CHECK(transport.send(tempo_request()).delivery == LomDeliveryState::NotSent);
    peer.wait_for_closed_sessions(1);
    CHECK(complete_requests == 1);
}

TEST_CASE("Owning absolute request deadline caps the configured native exchange budget",
          "[bridge][transport][loopback][request-control]") {
    std::atomic<int> requests{0};
    ScriptedPeer peer({[&](int client) {
        const auto identity = read_frame(client);
        if (!identity || !answer_identity_request(client, *identity)) return;
        if (read_frame(client)) ++requests;
        (void)read_frame(client);
    }});
    TcpTransport transport(loopback_config(peer.port()));
    REQUIRE(transport.connect());
    auto control = std::make_shared<RequestControl>(
        "number:4", "tools/call", std::chrono::steady_clock::now() + 100ms);
    RequestControlScope scope(control);
    const auto started = std::chrono::steady_clock::now();
    const auto unknown = transport.send(tempo_request());
    CHECK_FALSE(unknown.success);
    CHECK(unknown.delivery == LomDeliveryState::SentWithoutValidResponse);
    CHECK(std::chrono::steady_clock::now() - started < 350ms);
    peer.wait_for_closed_sessions(1);
    CHECK(requests == 1);
}

TEST_CASE("Cancelling actual DNS cleanup reaps the exact owned child",
          "[bridge][transport][resolution][request-control]") {
    ResolverPeer resolver("time.sleep(30)");
    auto config = loopback_config(9001, "sunny-cancel-resolver.invalid");
    config.resolver_executable = resolver.path();
    config.connect_timeout = 2s;
    auto control = std::make_shared<RequestControl>("number:5", "tools/call");
    std::jthread revoke([&] {
        const auto until = std::chrono::steady_clock::now() + 750ms;
        struct stat status {};
        while (::stat((resolver.path() + ".pid").c_str(), &status) != 0 &&
               std::chrono::steady_clock::now() < until)
            std::this_thread::sleep_for(1ms);
        control->cancelled = true;
    });
    RequestControlScope scope(control);
    TcpTransport transport(config);
    const auto started = std::chrono::steady_clock::now();
    CHECK_FALSE(transport.connect());
    CHECK(transport.last_connect_failure() == ConnectFailure::RequestRevoked);
    CHECK(std::chrono::steady_clock::now() - started < 1s);
    revoke.join();
    resolver.check_reaped();
}
