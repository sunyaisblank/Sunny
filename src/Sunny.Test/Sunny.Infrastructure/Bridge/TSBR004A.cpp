/**
 * @file TSBR004A.cpp
 * @brief Unit tests for INBR002A (BridgeDispatcher) and the TCP loopback path
 *
 * Component: TSBR004A
 * Domain: TS (Test) | Category: BR (Bridge)
 *
 * Tests: INBR002A, INTP001A (TcpTransport)
 * Coverage: BridgeMessage → LomRequest translation, AddNotes routing,
 *           offline decline, framed request/response over a real socket
 */

#include <catch2/catch_test_macros.hpp>

#include "Bridge/INBR002A.h"
#include "Bridge/INTP001A.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace Sunny::Infrastructure;

// =============================================================================
// Translation table
// =============================================================================

TEST_CASE("TSBR004A: CreateClip translates to create_clip call with duration",
          "[bridge][dispatcher]") {
    BridgeMessage msg;
    msg.type = BridgeMessageType::CreateClip;
    msg.path = "tracks/2/clip_slots/3";
    msg.args.push_back("4.5");

    auto req = BridgeDispatcher::to_lom_request(msg);

    REQUIRE(req.type == LomRequestType::CallMethod);
    REQUIRE(req.path.to_string() == "tracks/2/clip_slots/3");
    REQUIRE(req.property_or_method == "create_clip");
    REQUIRE(req.args.size() == 1);
    REQUIRE(std::get<double>(req.args[0]) == 4.5);
}

TEST_CASE("TSBR004A: CallMethod takes method from first arg, rest as args",
          "[bridge][dispatcher]") {
    BridgeMessage msg;
    msg.type = BridgeMessageType::CallMethod;
    msg.path = "tracks/0/clip_slots/1";
    msg.args = {"delete_clip"};

    auto req = BridgeDispatcher::to_lom_request(msg);

    REQUIRE(req.type == LomRequestType::CallMethod);
    REQUIRE(req.property_or_method == "delete_clip");
    REQUIRE(req.args.empty());
}

TEST_CASE("TSBR004A: SetProperty and GetProperty carry the property name",
          "[bridge][dispatcher]") {
    BridgeMessage set_msg;
    set_msg.type = BridgeMessageType::SetProperty;
    set_msg.path = "tracks/1";
    set_msg.args = {"mute", "1"};

    auto set_req = BridgeDispatcher::to_lom_request(set_msg);
    REQUIRE(set_req.type == LomRequestType::SetProperty);
    REQUIRE(set_req.property_or_method == "mute");
    REQUIRE(std::get<std::string>(set_req.args[0]) == "1");

    BridgeMessage get_msg;
    get_msg.type = BridgeMessageType::GetProperty;
    get_msg.path = "song";
    get_msg.args = {"tempo"};

    auto get_req = BridgeDispatcher::to_lom_request(get_msg);
    REQUIRE(get_req.type == LomRequestType::GetProperty);
    REQUIRE(get_req.property_or_method == "tempo");
}

// =============================================================================
// Dispatch through a recording transport
// =============================================================================

TEST_CASE("TSBR004A: dispatch routes AddNotes through send_notes",
          "[bridge][dispatcher]") {
    CommandBuffer buffer;
    BridgeDispatcher dispatcher(&buffer);
    REQUIRE(dispatcher.online());

    Sunny::Core::NoteEvent note;
    note.pitch = 60;
    note.velocity = 100;

    BridgeMessage create_msg;
    create_msg.type = BridgeMessageType::CreateClip;
    create_msg.path = "tracks/0/clip_slots/0";
    create_msg.args.push_back("4.0");

    BridgeMessage notes_msg;
    notes_msg.type = BridgeMessageType::AddNotes;
    notes_msg.path = "tracks/0/clip_slots/0/clip";
    notes_msg.notes.push_back(note);

    auto report = dispatcher.dispatch({create_msg, notes_msg});

    REQUIRE(report.all_ok());
    REQUIRE(report.sent == 2);
    REQUIRE(buffer.size() == 2);
    REQUIRE(buffer.entries()[0].request.property_or_method == "create_clip");
    REQUIRE(buffer.entries()[1].notes.size() == 1);
    REQUIRE(buffer.entries()[1].notes[0].pitch == 60);
}

TEST_CASE("TSBR004A: offline dispatcher declines every message",
          "[bridge][dispatcher]") {
    BridgeDispatcher dispatcher(nullptr);
    REQUIRE_FALSE(dispatcher.online());

    BridgeMessage msg;
    msg.type = BridgeMessageType::CreateClip;
    msg.path = "tracks/0/clip_slots/0";
    msg.args.push_back("4.0");

    auto report = dispatcher.dispatch({msg, msg});

    REQUIRE_FALSE(report.all_ok());
    REQUIRE(report.sent == 0);
    REQUIRE(report.failed == 2);
    REQUIRE_FALSE(report.errors.empty());
}

// =============================================================================
// TCP loopback: framed request/response against a scripted server
// =============================================================================

namespace {

/// Minimal scripted server speaking the SunnyRemoteScript wire protocol:
/// accepts one client, answers every framed request with a canned response.
class LoopbackServer {
public:
    explicit LoopbackServer(std::string response_json)
        : response_(std::move(response_json)) {
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(listen_fd_ >= 0);

        int reuse = 1;
        ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;  // ephemeral
        REQUIRE(::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr),
                       sizeof(addr)) == 0);
        REQUIRE(::listen(listen_fd_, 1) == 0);

        socklen_t len = sizeof(addr);
        REQUIRE(::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr),
                              &len) == 0);
        port_ = ntohs(addr.sin_port);

        thread_ = std::thread([this] { serve(); });
    }

    ~LoopbackServer() {
        stop_ = true;
        if (listen_fd_ >= 0) ::close(listen_fd_);
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] std::uint16_t port() const { return port_; }
    [[nodiscard]] std::size_t requests_served() const { return served_; }

private:
    void serve() {
        int client = ::accept(listen_fd_, nullptr, nullptr);
        if (client < 0) return;

        while (!stop_) {
            std::uint32_t net_len = 0;
            if (!recv_exact(client, &net_len, sizeof(net_len))) break;
            std::uint32_t payload_len = ntohl(net_len);

            std::string payload(payload_len, '\0');
            if (!recv_exact(client, payload.data(), payload_len)) break;
            ++served_;

            std::uint32_t resp_len = htonl(
                static_cast<std::uint32_t>(response_.size()));
            ::send(client, &resp_len, sizeof(resp_len), 0);
            ::send(client, response_.data(), response_.size(), 0);
        }
        ::close(client);
    }

    static bool recv_exact(int fd, void* buf, std::size_t n) {
        auto* p = static_cast<char*>(buf);
        std::size_t got = 0;
        while (got < n) {
            ssize_t r = ::recv(fd, p + got, n - got, 0);
            if (r <= 0) return false;
            got += static_cast<std::size_t>(r);
        }
        return true;
    }

    std::string response_;
    int listen_fd_{-1};
    std::uint16_t port_{0};
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<std::size_t> served_{0};
};

}  // namespace

TEST_CASE("TSBR004A: TcpTransport round-trips framed JSON over loopback",
          "[bridge][transport][loopback]") {
    LoopbackServer server(R"({"success": true, "value": 120.0})");

    TcpConfig config;
    config.host = "127.0.0.1";
    config.port = server.port();

    TcpTransport transport(config);
    REQUIRE(transport.connect());
    REQUIRE(transport.is_connected());

    auto response = transport.send(
        LomProtocol::get_property(LomPaths::song(), "tempo"));

    REQUIRE(response.success);
    REQUIRE(response.value.has_value());
    REQUIRE(std::get<double>(*response.value) == 120.0);

    transport.disconnect();
    REQUIRE_FALSE(transport.is_connected());
}

TEST_CASE("TSBR004A: full dispatch path delivers orchestrator messages "
          "over loopback",
          "[bridge][transport][loopback]") {
    LoopbackServer server(R"({"success": true})");

    TcpConfig config;
    config.host = "127.0.0.1";
    config.port = server.port();

    TcpTransport transport(config);
    REQUIRE(transport.connect());

    BridgeDispatcher dispatcher(&transport);
    REQUIRE(dispatcher.online());

    Sunny::Core::NoteEvent note;
    note.pitch = 64;
    note.velocity = 90;

    BridgeMessage create_msg;
    create_msg.type = BridgeMessageType::CreateClip;
    create_msg.path = "tracks/0/clip_slots/0";
    create_msg.args.push_back("4.0");

    BridgeMessage notes_msg;
    notes_msg.type = BridgeMessageType::AddNotes;
    notes_msg.path = "tracks/0/clip_slots/0/clip";
    notes_msg.notes.push_back(note);

    auto report = dispatcher.dispatch({create_msg, notes_msg});

    REQUIRE(report.all_ok());
    REQUIRE(report.sent == 2);
    REQUIRE(server.requests_served() == 2);
}

TEST_CASE("TSBR004A: transport declines cleanly when nothing is listening",
          "[bridge][transport]") {
    TcpConfig config;
    config.host = "127.0.0.1";
    config.port = 1;  // reserved port: connection refused
    config.connect_timeout = std::chrono::milliseconds{500};

    TcpTransport transport(config);
    REQUIRE_FALSE(transport.connect());
    REQUIRE_FALSE(transport.is_connected());

    auto response = transport.send(
        LomProtocol::get_property(LomPaths::song(), "tempo"));
    REQUIRE_FALSE(response.success);
    REQUIRE(response.error.has_value());
}
