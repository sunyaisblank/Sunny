/**
 * @file dispatcher_test.cpp
 * @brief BridgeDispatcher unit tests and the TCP loopback path
 *
 *
 * Coverage: BridgeMessage → LomRequest translation, AddNotes routing,
 *           offline decline, framed request/response over a real socket
 */

#include <arpa/inet.h>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <sunny/infrastructure/ableton/dispatcher.hpp>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace sunny::infrastructure;

// =============================================================================
// Translation table
// =============================================================================

TEST_CASE("CreateClip translates to create_clip call with duration", "[bridge][dispatcher]") {
    BridgeMessage msg;
    msg.type = BridgeMessageType::CreateClip;
    msg.path = "song/tracks/2/clip_slots/3";
    msg.args.push_back("4.5");

    auto req = BridgeDispatcher::to_lom_request(msg);

    REQUIRE(req.has_value());
    REQUIRE(req->type == LomRequestType::CallMethod);
    REQUIRE(req->path.to_string() == "song/tracks/2/clip_slots/3");
    REQUIRE(req->property_or_method == "create_clip");
    REQUIRE(req->args.size() == 1);
    REQUIRE(std::get<double>(req->args[0]) == 4.5);
}

TEST_CASE("CallMethod takes method from first arg, rest as args", "[bridge][dispatcher]") {
    BridgeMessage msg;
    msg.type = BridgeMessageType::CallMethod;
    msg.path = "song/tracks/0/clip_slots/1";
    msg.args = {"delete_clip"};

    auto req = BridgeDispatcher::to_lom_request(msg);

    REQUIRE(req.has_value());
    REQUIRE(req->type == LomRequestType::CallMethod);
    REQUIRE(req->property_or_method == "delete_clip");
    REQUIRE(req->args.empty());
}

TEST_CASE("SetProperty and GetProperty carry the property name", "[bridge][dispatcher]") {
    BridgeMessage set_msg;
    set_msg.type = BridgeMessageType::SetProperty;
    set_msg.path = "song/tracks/1";
    set_msg.args = {"mute", "1"};

    auto set_req = BridgeDispatcher::to_lom_request(set_msg);
    REQUIRE(set_req.has_value());
    REQUIRE(set_req->type == LomRequestType::SetProperty);
    REQUIRE(set_req->property_or_method == "mute");
    REQUIRE(std::get<std::string>(set_req->args[0]) == "1");

    BridgeMessage get_msg;
    get_msg.type = BridgeMessageType::GetProperty;
    get_msg.path = "song";
    get_msg.args = {"tempo"};

    auto get_req = BridgeDispatcher::to_lom_request(get_msg);
    REQUIRE(get_req.has_value());
    REQUIRE(get_req->type == LomRequestType::GetProperty);
    REQUIRE(get_req->property_or_method == "tempo");
}

TEST_CASE("dispatcher translation rejects malformed bridge messages before transport",
          "[bridge][dispatcher][trust-boundary]") {
    BridgeMessage message;
    message.type = BridgeMessageType::CreateClip;
    message.path = "song/tracks/0/clip_slots/0";

    CHECK_FALSE(BridgeDispatcher::to_lom_request(message));
    message.args = {"4beats"};
    CHECK_FALSE(BridgeDispatcher::to_lom_request(message));
    message.args = {"nan"};
    CHECK_FALSE(BridgeDispatcher::to_lom_request(message));
    message.args = {"4.0"};
    message.path = "song/tracks/00/clip_slots/0";
    CHECK_FALSE(BridgeDispatcher::to_lom_request(message));
}

// =============================================================================
// Dispatch through a recording transport
// =============================================================================

TEST_CASE("dispatch routes AddNotes through send_notes", "[bridge][dispatcher]") {
    CommandBuffer buffer;
    BridgeDispatcher dispatcher(&buffer);
    REQUIRE(dispatcher.online());

    sunny::core::NoteEvent note;
    note.pitch = 60;
    note.start_time = sunny::core::Beat{1, 4};
    note.duration = sunny::core::Beat{1, 8};
    note.velocity = 100;

    BridgeMessage create_msg;
    create_msg.type = BridgeMessageType::CreateClip;
    create_msg.path = "song/tracks/0/clip_slots/0";
    create_msg.args.push_back("4.0");

    BridgeMessage notes_msg;
    notes_msg.type = BridgeMessageType::AddNotes;
    notes_msg.path = "song/tracks/0/clip_slots/0/clip";
    notes_msg.notes.push_back(note);

    auto report = dispatcher.dispatch({create_msg, notes_msg});

    REQUIRE(report.all_ok());
    REQUIRE(report.sent == 2);
    REQUIRE(buffer.size() == 2);
    REQUIRE(buffer.entries()[0].request.property_or_method == "create_clip");
    REQUIRE(buffer.entries()[1].request.property_or_method == "add_new_notes");
    REQUIRE(buffer.entries()[1].notes.size() == 1);
    REQUIRE(buffer.entries()[1].notes[0].pitch == 60);
    REQUIRE(buffer.entries()[1].notes[0].start_time == 1.0);
    REQUIRE(buffer.entries()[1].notes[0].duration == 0.5);
}

TEST_CASE("offline dispatcher declines every message", "[bridge][dispatcher]") {
    BridgeDispatcher dispatcher(nullptr);
    REQUIRE_FALSE(dispatcher.online());

    BridgeMessage msg;
    msg.type = BridgeMessageType::CreateClip;
    msg.path = "song/tracks/0/clip_slots/0";
    msg.args.push_back("4.0");

    auto report = dispatcher.dispatch({msg, msg});

    REQUIRE_FALSE(report.all_ok());
    REQUIRE(report.sent == 0);
    REQUIRE(report.failed == 2);
    REQUIRE_FALSE(report.errors.empty());
}

TEST_CASE("dispatcher declines malformed AddNotes without recording a send",
          "[bridge][dispatcher][trust-boundary]") {
    CommandBuffer buffer;
    BridgeDispatcher dispatcher(&buffer);

    BridgeMessage message;
    message.type = BridgeMessageType::AddNotes;
    message.path = "song/tracks/0/clip_slots/0/clip";
    message.notes.push_back({60, sunny::core::Beat{0, 1}, sunny::core::Beat{0, 1}, 100, false});

    const auto report = dispatcher.dispatch({message});
    CHECK_FALSE(report.all_ok());
    CHECK(report.sent == 0);
    CHECK(report.failed == 1);
    CHECK(buffer.size() == 0);
}

namespace {

/// Acknowledges every request except the one at a chosen position in the stream.
class FailAtTransport final : public LomTransport {
  public:
    FailAtTransport(std::size_t failing_index, LomDeliveryState delivery)
        : failing_index_(failing_index), delivery_(delivery) {}

    LomResponse send(const LomRequest& request) override {
        methods.push_back(request.property_or_method);
        return respond();
    }

    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override {
        methods.emplace_back("add_new_notes");
        return respond();
    }

    [[nodiscard]] bool is_connected() const override { return true; }

    std::vector<std::string> methods;

  private:
    LomResponse respond() {
        if (methods.size() - 1 == failing_index_)
            return {false, std::nullopt, std::string{"injected failure"}, delivery_};
        return {true, std::nullopt, std::nullopt};
    }

    std::size_t failing_index_;
    LomDeliveryState delivery_;
};

std::vector<BridgeMessage> create_notes_delete_batch() {
    BridgeMessage create_msg;
    create_msg.type = BridgeMessageType::CreateClip;
    create_msg.path = "song/tracks/0/clip_slots/0";
    create_msg.args.push_back("4.0");

    BridgeMessage notes_msg;
    notes_msg.type = BridgeMessageType::AddNotes;
    notes_msg.path = "song/tracks/0/clip_slots/0/clip";
    notes_msg.notes.push_back({60, sunny::core::Beat{0, 1}, sunny::core::Beat{1, 4}, 100, false});

    BridgeMessage delete_msg;
    delete_msg.type = BridgeMessageType::CallMethod;
    delete_msg.path = "song/tracks/0/clip_slots/1";
    delete_msg.args = {"delete_clip"};
    return {create_msg, notes_msg, delete_msg};
}

} // namespace

TEST_CASE("dispatch stops at the first failed message", "[bridge][dispatcher][atomicity]") {
    FailAtTransport transport(0, LomDeliveryState::ResponseReceived);
    BridgeDispatcher dispatcher(&transport);

    const auto report = dispatcher.dispatch(create_notes_delete_batch());

    CHECK_FALSE(report.all_ok());
    CHECK(report.sent == 0);
    CHECK(report.failed == 3);
    CHECK_FALSE(report.indeterminate);
    CHECK(transport.methods == std::vector<std::string>{"create_clip"});
    REQUIRE(report.errors.size() == 2);
    CHECK(report.errors[0].find("injected failure") != std::string::npos);
}

TEST_CASE("dispatch reports the acknowledged prefix and an indeterminate failure",
          "[bridge][dispatcher][atomicity]") {
    FailAtTransport transport(1, LomDeliveryState::SentWithoutValidResponse);
    BridgeDispatcher dispatcher(&transport);

    const auto report = dispatcher.dispatch(create_notes_delete_batch());

    CHECK(report.sent == 1);
    CHECK(report.failed == 2);
    CHECK(report.indeterminate);
    CHECK(transport.methods == std::vector<std::string>{"create_clip", "add_new_notes"});
}

// =============================================================================
// TCP loopback: framed request/response against a scripted server
// =============================================================================

namespace {

/// Minimal scripted server speaking the Sunny Remote Script wire protocol:
/// accepts one client, answers every framed request with a canned response.
class LoopbackServer {
  public:
    explicit LoopbackServer(std::string response_json, std::uint16_t requested_port = 0)
        : response_(std::move(response_json)) {
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(listen_fd_ >= 0);

        int reuse = 1;
        ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(requested_port); // zero selects an ephemeral port
        REQUIRE(::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
        REQUIRE(::listen(listen_fd_, 1) == 0);

        socklen_t len = sizeof(addr);
        REQUIRE(::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len) == 0);
        port_ = ntohs(addr.sin_port);

        thread_ = std::thread([this] { serve(); });
    }

    ~LoopbackServer() {
        stop_ = true;
        const int client = client_fd_.exchange(-1);
        if (client >= 0) {
            ::shutdown(client, SHUT_RDWR);
            ::close(client);
        }
        if (listen_fd_ >= 0) ::close(listen_fd_);
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] std::uint16_t port() const { return port_; }
    [[nodiscard]] std::size_t requests_served() const { return served_; }

  private:
    void serve() {
        int client = ::accept(listen_fd_, nullptr, nullptr);
        if (client < 0) return;
        client_fd_ = client;

        while (!stop_) {
            std::uint32_t net_len = 0;
            if (!recv_exact(client, &net_len, sizeof(net_len))) break;
            std::uint32_t payload_len = ntohl(net_len);

            std::string payload(payload_len, '\0');
            if (!recv_exact(client, payload.data(), payload_len)) break;
            ++served_;

            std::uint32_t resp_len = htonl(static_cast<std::uint32_t>(response_.size()));
            ::send(client, &resp_len, sizeof(resp_len), 0);
            ::send(client, response_.data(), response_.size(), 0);
        }
        if (client_fd_.exchange(-1) >= 0) ::close(client);
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
    std::atomic<int> client_fd_{-1};
};

std::uint16_t unused_loopback_port() {
    const int socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(socket_fd >= 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    REQUIRE(::bind(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);

    socklen_t len = sizeof(addr);
    REQUIRE(::getsockname(socket_fd, reinterpret_cast<sockaddr*>(&addr), &len) == 0);
    const auto port = ntohs(addr.sin_port);
    ::close(socket_fd);
    return port;
}

} // namespace

TEST_CASE("TcpTransport round-trips framed JSON over loopback", "[bridge][transport][loopback]") {
    LoopbackServer server(R"({"bridge_protocol_version":46,"success":true,"value":120.0})");

    TcpConfig config;
    config.host = "127.0.0.1";
    config.port = server.port();

    TcpTransport transport(config);
    REQUIRE(transport.connect());
    REQUIRE(transport.is_connected());

    auto response = transport.send(LomProtocol::get_property(LomPaths::song(), "tempo"));

    REQUIRE(response.success);
    REQUIRE(response.value.has_value());
    REQUIRE(std::get<double>(*response.value) == 120.0);

    transport.disconnect();
    REQUIRE_FALSE(transport.is_connected());
}

TEST_CASE("TcpTransport rejects an oversized request before writing a frame",
          "[bridge][transport][loopback][limits]") {
    LoopbackServer server(R"({"bridge_protocol_version":46,"success":true})");

    TcpConfig config;
    config.host = "127.0.0.1";
    config.port = server.port();
    TcpTransport transport(config);
    REQUIRE(transport.connect());

    const std::string oversized_name(SUNNY_BRIDGE_MAX_WIRE_PAYLOAD, 'x');
    const auto response = transport.send(
        LomProtocol::call_method(LomPaths::track(0), "insert_device", {oversized_name}));

    CHECK_FALSE(response.success);
    REQUIRE(response.error.has_value());
    CHECK(*response.error == "request too large");
    CHECK(response.delivery == LomDeliveryState::NotSent);
    CHECK(server.requests_served() == 0);
    CHECK(transport.is_connected());
}

TEST_CASE("TcpTransport validates the target-profile handshake over the real wire",
          "[bridge][transport][loopback][target-profile]") {
    const auto expected = modeled_target_profile({12, 3, 5, "12.3.5"});
    const auto wire_response =
        nlohmann::json{{"bridge_protocol_version", SUNNY_BRIDGE_PROTOCOL_VERSION},
                       {"success", true},
                       {"value", target_profile_to_json(expected)}}
            .dump();
    LoopbackServer server(wire_response);

    TcpConfig config;
    config.host = "127.0.0.1";
    config.port = server.port();
    TcpTransport transport(config);
    REQUIRE(transport.connect());

    auto profile = transport.target_profile();
    REQUIRE(profile.has_value());
    REQUIRE(profile->has_value());
    CHECK((*profile)->live_version.version_string == "12.3.5");
    CHECK((*profile)->native_device_insertion == CapabilityState::Available);
}

TEST_CASE("TcpTransport validates a structural target snapshot over the real wire",
          "[bridge][transport][loopback][target-snapshot]") {
    const auto parameter = nlohmann::json{{"value", 0.5},
                                          {"display_value", 0.0},
                                          {"minimum", 0.0},
                                          {"maximum", 1.0},
                                          {"is_quantized", false},
                                          {"default_value", 0.0},
                                          {"value_items", nullptr},
                                          {"state", 0},
                                          {"automation_state", 0},
                                          {"is_enabled", true}};
    auto activator = parameter;
    activator["value"] = 1.0;
    activator["display_value"] = 1.0;
    activator["is_quantized"] = true;
    activator["default_value"] = nullptr;
    activator["value_items"] = nlohmann::json::array({"Off", "On"});
    auto panning = parameter;
    panning["minimum"] = -1.0;
    AbletonTargetSnapshot expected;
    expected.target_profile = modeled_target_profile({12, 3, 5, "12.3.5"});
    expected.song_state = {
        {"tempo", 120.0},
        {"signature_numerator", 4},
        {"signature_denominator", 4},
        {"is_playing", false},
        {"is_counting_in", false},
        {"arrangement_overdub", false},
        {"overdub", false},
        {"record_mode", false},
        {"session_record", false},
        {"session_automation_record", false},
        {"is_ableton_link_enabled", false},
        {"is_ableton_link_start_stop_sync_enabled", false},
        {"tempo_follower_enabled", false},
        {"nudge_down", false},
        {"nudge_up", false},
        {"back_to_arranger", false},
        {"re_enable_automation_enabled", false},
        {"loop", false},
        {"metronome", false},
        {"scale",
         {{"root_note", 0},
          {"name", "Chromatic"},
          {"intervals", nlohmann::json::array({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11})},
          {"mode", false}}},
        {"tuning_system",
         {{"name", "12-TET"},
          {"pseudo_octave_in_cents", 1200.0},
          {"lowest_note", {{"opaque_fixture", "lowest"}}},
          {"highest_note", {{"opaque_fixture", "highest"}}},
          {"reference_pitch", {{"opaque_fixture", "reference"}}},
          {"note_tunings",
           {{"opaque_fixture",
             nlohmann::json::array({0.0,
                                    100.0,
                                    200.0,
                                    300.0,
                                    400.0,
                                    500.0,
                                    600.0,
                                    700.0,
                                    800.0,
                                    900.0,
                                    1000.0,
                                    1100.0})}}}}},
        {"scene_count", 1},
        {"scenes",
         nlohmann::json::array({{{"name", "Scene 1"},
                                 {"is_triggered", false},
                                 {"tempo_enabled", false},
                                 {"tempo", -1.0},
                                 {"time_signature_enabled", false},
                                 {"time_signature_numerator", -1},
                                 {"time_signature_denominator", -1}}})},
        {"tracks", nlohmann::json::array()},
        {"return_tracks", nlohmann::json::array()},
        {"master_track",
         {{"name", "Master"},
          {"devices", nlohmann::json::array()},
          {"mixer",
           {{"volume", parameter},
            {"track_activator", activator},
            {"panning", panning},
            {"sends", nlohmann::json::array()},
            {"crossfade_assign", nullptr},
            {"panning_mode", 0}}}}},
        {"cue_points", nlohmann::json::array()}};
    const auto wire_response =
        nlohmann::json{{"bridge_protocol_version", SUNNY_BRIDGE_PROTOCOL_VERSION},
                       {"success", true},
                       {"value", target_snapshot_to_json(expected)}}
            .dump();
    LoopbackServer server(wire_response);

    TcpConfig config;
    config.host = "127.0.0.1";
    config.port = server.port();
    TcpTransport transport(config);
    REQUIRE(transport.connect());

    const auto snapshot = transport.target_snapshot();
    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->has_value());
    CHECK(equivalent_target_snapshot(**snapshot, expected));
}

TEST_CASE("TcpTransport reads existing return count over the real wire",
          "[bridge][transport][loopback][target-state]") {
    LoopbackServer server(R"({"bridge_protocol_version":46,"success":true,"value":2})");

    TcpConfig config;
    config.host = "127.0.0.1";
    config.port = server.port();
    TcpTransport transport(config);
    REQUIRE(transport.connect());

    auto count = transport.return_track_count();
    REQUIRE(count.has_value());
    REQUIRE(count->has_value());
    CHECK(**count == 2);
}

TEST_CASE("TcpTransport reads existing device count over the real wire",
          "[bridge][transport][loopback][target-state]") {
    LoopbackServer server(R"({"bridge_protocol_version":46,"success":true,"value":7})");

    TcpConfig config;
    config.host = "127.0.0.1";
    config.port = server.port();
    TcpTransport transport(config);
    REQUIRE(transport.connect());

    auto count = transport.device_count(LomPaths::track(3));
    REQUIRE(count.has_value());
    REQUIRE(count->has_value());
    CHECK(**count == 7);
}

TEST_CASE("TcpTransport reads Session scene count over the real wire",
          "[bridge][transport][loopback][target-state]") {
    LoopbackServer server(R"({"bridge_protocol_version":46,"success":true,"value":3})");

    TcpConfig config;
    config.host = "127.0.0.1";
    config.port = server.port();
    TcpTransport transport(config);
    REQUIRE(transport.connect());

    auto count = transport.scene_count();
    REQUIRE(count.has_value());
    REQUIRE(count->has_value());
    CHECK(**count == 3);
}

TEST_CASE("TcpTransport rejects pseudo-collection and negative count responses",
          "[bridge][transport][loopback][target-state][trust-boundary]") {
    SECTION("object labels are not a count") {
        LoopbackServer server(
            R"({"bridge_protocol_version":46,"success":true,"value":["Scene 1"]})");
        TcpConfig config;
        config.host = "127.0.0.1";
        config.port = server.port();
        TcpTransport transport(config);
        REQUIRE(transport.connect());
        CHECK(transport.scene_count().error() == sunny::core::ErrorCode::ProtocolError);
    }

    SECTION("negative count") {
        LoopbackServer server(R"({"bridge_protocol_version":46,"success":true,"value":-1})");
        TcpConfig config;
        config.host = "127.0.0.1";
        config.port = server.port();
        TcpTransport transport(config);
        REQUIRE(transport.connect());
        CHECK(transport.return_track_count().error() == sunny::core::ErrorCode::ProtocolError);
    }
}

TEST_CASE("full dispatch path delivers orchestrator messages "
          "over loopback",
          "[bridge][transport][loopback]") {
    LoopbackServer server(R"({"bridge_protocol_version":46,"success":true})");

    TcpConfig config;
    config.host = "127.0.0.1";
    config.port = server.port();

    TcpTransport transport(config);
    REQUIRE(transport.connect());

    BridgeDispatcher dispatcher(&transport);
    REQUIRE(dispatcher.online());

    sunny::core::NoteEvent note;
    note.pitch = 64;
    note.start_time = sunny::core::Beat{0, 1};
    note.duration = sunny::core::Beat{1, 4};
    note.velocity = 90;

    BridgeMessage create_msg;
    create_msg.type = BridgeMessageType::CreateClip;
    create_msg.path = "song/tracks/0/clip_slots/0";
    create_msg.args.push_back("4.0");

    BridgeMessage notes_msg;
    notes_msg.type = BridgeMessageType::AddNotes;
    notes_msg.path = "song/tracks/0/clip_slots/0/clip";
    notes_msg.notes.push_back(note);

    auto report = dispatcher.dispatch({create_msg, notes_msg});

    REQUIRE(report.all_ok());
    REQUIRE(report.sent == 2);
    REQUIRE(server.requests_served() == 2);
}

TEST_CASE("transport declines cleanly when nothing is listening", "[bridge][transport]") {
    TcpConfig config;
    config.host = "127.0.0.1";
    config.port = 1; // reserved port: connection refused
    config.connect_timeout = std::chrono::milliseconds{500};

    TcpTransport transport(config);
    REQUIRE_FALSE(transport.connect());
    REQUIRE_FALSE(transport.is_connected());

    auto response = transport.send(LomProtocol::get_property(LomPaths::song(), "tempo"));
    REQUIRE_FALSE(response.success);
    REQUIRE(response.error.has_value());
}

TEST_CASE("dispatcher reconnects when Remote Script starts later",
          "[bridge][transport][loopback]") {
    const auto port = unused_loopback_port();
    TcpConfig config;
    config.host = "127.0.0.1";
    config.port = port;
    config.connect_timeout = std::chrono::milliseconds{100};

    TcpTransport transport(config);
    REQUIRE_FALSE(transport.connect());
    LoopbackServer server(R"({"bridge_protocol_version":46,"success":true,"value":121.0})", port);

    BridgeDispatcher dispatcher(&transport);
    REQUIRE(dispatcher.online());
    auto response = dispatcher.request(LomProtocol::get_property(LomPaths::song(), "tempo"));
    REQUIRE(response.success);
    CHECK(std::get<double>(*response.value) == 121.0);
}

// The Remote Script closes its side of the idle connection when Live reloads
// it. Nothing was in flight, so the transport must reconnect before sending:
// the first request after the restart succeeds rather than paying for the
// stale socket.
TEST_CASE("the first request after a Remote Script restart succeeds",
          "[bridge][transport][loopback][lifecycle]") {
    const auto port = unused_loopback_port();
    TcpConfig config;
    config.host = "127.0.0.1";
    config.port = port;
    config.connect_timeout = std::chrono::milliseconds{100};

    auto server = std::make_unique<LoopbackServer>(
        R"({"bridge_protocol_version":46,"success":true,"value":120.0})", port);
    TcpTransport transport(config);
    REQUIRE(transport.connect());
    BridgeDispatcher dispatcher(&transport);
    REQUIRE(dispatcher.request(LomProtocol::get_property(LomPaths::song(), "tempo")).success);

    server.reset();
    server = std::make_unique<LoopbackServer>(
        R"({"bridge_protocol_version":46,"success":true,"value":122.0})", port);
    auto recovered = dispatcher.request(LomProtocol::get_property(LomPaths::song(), "tempo"));
    REQUIRE(recovered.success);
    CHECK(recovered.delivery == LomDeliveryState::ResponseReceived);
    CHECK(std::get<double>(*recovered.value) == 122.0);
}

// While the Remote Script is absent, the stale socket is detected before any
// byte is written, so the decline is a definite NotSent rather than an
// ambiguous delivery; recovery follows once the script returns.
TEST_CASE("a request while the Remote Script is absent is declined as not sent",
          "[bridge][transport][loopback][lifecycle]") {
    const auto port = unused_loopback_port();
    TcpConfig config;
    config.host = "127.0.0.1";
    config.port = port;
    config.connect_timeout = std::chrono::milliseconds{100};

    auto server = std::make_unique<LoopbackServer>(
        R"({"bridge_protocol_version":46,"success":true,"value":120.0})", port);
    TcpTransport transport(config);
    REQUIRE(transport.connect());
    BridgeDispatcher dispatcher(&transport);
    REQUIRE(dispatcher.request(LomProtocol::get_property(LomPaths::song(), "tempo")).success);

    server.reset();
    auto declined = dispatcher.request(LomProtocol::get_property(LomPaths::song(), "tempo"));
    REQUIRE_FALSE(declined.success);
    CHECK(declined.delivery == LomDeliveryState::NotSent);
    REQUIRE_FALSE(transport.is_connected());

    server = std::make_unique<LoopbackServer>(
        R"({"bridge_protocol_version":46,"success":true,"value":122.0})", port);
    auto recovered = dispatcher.request(LomProtocol::get_property(LomPaths::song(), "tempo"));
    REQUIRE(recovered.success);
    CHECK(std::get<double>(*recovered.value) == 122.0);
}
