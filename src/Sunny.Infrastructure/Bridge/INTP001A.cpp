/**
 * @file INTP001A.cpp
 * @brief LOM Bridge Transport Layer implementation
 *
 * Component: INTP001A
 *
 * CommandBuffer: fully implemented (records commands for testing).
 * TcpTransport: structural placeholder (connect/send return error
 * responses until platform networking is integrated).
 */

#include "INTP001A.h"

#include <cerrno>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace Sunny::Infrastructure {

// =============================================================================
// CommandBuffer
// =============================================================================

LomResponse CommandBuffer::send(const LomRequest& request) {
    Entry entry;
    entry.request = request;
    entries_.push_back(std::move(entry));
    return LomResponse{true, std::nullopt, std::nullopt, std::nullopt};
}

LomResponse CommandBuffer::send_notes(
    const LomPath& clip_path,
    const std::vector<LomNoteData>& notes
) {
    Entry entry;
    entry.request.type = LomRequestType::CallMethod;
    entry.request.path = clip_path;
    entry.request.property_or_method = "add_notes";
    entry.notes = notes;
    entries_.push_back(std::move(entry));
    return LomResponse{true, std::nullopt, std::nullopt, std::nullopt};
}

std::vector<const CommandBuffer::Entry*> CommandBuffer::find_by_type(
    LomRequestType type
) const {
    std::vector<const Entry*> result;
    for (const auto& e : entries_) {
        if (e.request.type == type)
            result.push_back(&e);
    }
    return result;
}

std::vector<const CommandBuffer::Entry*> CommandBuffer::find_by_path_prefix(
    const std::string& prefix
) const {
    std::vector<const Entry*> result;
    for (const auto& e : entries_) {
        auto path_str = e.request.path.to_string();
        if (path_str.substr(0, prefix.size()) == prefix)
            result.push_back(&e);
    }
    return result;
}

std::size_t CommandBuffer::count_type(LomRequestType type) const {
    std::size_t count = 0;
    for (const auto& e : entries_) {
        if (e.request.type == type)
            ++count;
    }
    return count;
}

// =============================================================================
// TcpTransport — POSIX socket implementation
// =============================================================================

TcpTransport::TcpTransport(const TcpConfig& config)
    : config_(config) {}

TcpTransport::~TcpTransport() {
    disconnect();
}

void TcpTransport::set_state(ConnectionState new_state) {
    state_ = new_state;
    if (state_callback_) state_callback_(state_);
}

bool TcpTransport::connect() {
    if (state_ == ConnectionState::Connected) return true;

    set_state(ConnectionState::Connecting);

    socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        set_state(ConnectionState::Error);
        return false;
    }

    // Set send/receive timeouts
    struct timeval tv;
    tv.tv_sec = config_.timeout.count() / 1000;
    tv.tv_usec = (config_.timeout.count() % 1000) * 1000;
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(socket_fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);

    if (inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr) <= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
        set_state(ConnectionState::Error);
        return false;
    }

    if (::connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr),
                  sizeof(addr)) < 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
        set_state(ConnectionState::Error);
        return false;
    }

    set_state(ConnectionState::Connected);
    return true;
}

void TcpTransport::disconnect() {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
    if (state_ != ConnectionState::Disconnected) {
        set_state(ConnectionState::Disconnected);
    }
}

bool TcpTransport::send_all(const void* data, std::size_t n) {
    const auto* ptr = static_cast<const char*>(data);
    std::size_t sent = 0;
    while (sent < n) {
        auto r = ::send(socket_fd_, ptr + sent, n - sent, MSG_NOSIGNAL);
        if (r <= 0) return false;
        sent += static_cast<std::size_t>(r);
    }
    return true;
}

bool TcpTransport::recv_all(void* data, std::size_t n) {
    auto* ptr = static_cast<char*>(data);
    std::size_t received = 0;
    while (received < n) {
        auto r = ::recv(socket_fd_, ptr + received, n - received, 0);
        if (r <= 0) return false;
        received += static_cast<std::size_t>(r);
    }
    return true;
}

LomResponse TcpTransport::send_and_receive(const std::string& json_payload) {
    // Send: 4-byte big-endian length + payload
    std::uint32_t len = static_cast<std::uint32_t>(json_payload.size());
    std::uint32_t net_len = htonl(len);

    if (!send_all(&net_len, 4) || !send_all(json_payload.data(), len)) {
        set_state(ConnectionState::Error);
        return LomResponse{false, std::nullopt,
                           std::string{"send failed"}, std::nullopt};
    }

    // Receive: 4-byte big-endian length + payload
    std::uint32_t resp_net_len = 0;
    if (!recv_all(&resp_net_len, 4)) {
        set_state(ConnectionState::Error);
        return LomResponse{false, std::nullopt,
                           std::string{"recv header failed"}, std::nullopt};
    }

    std::uint32_t resp_len = ntohl(resp_net_len);
    if (resp_len > 16 * 1024 * 1024) {
        set_state(ConnectionState::Error);
        return LomResponse{false, std::nullopt,
                           std::string{"response too large"}, std::nullopt};
    }

    std::string response_data(resp_len, '\0');
    if (!recv_all(response_data.data(), resp_len)) {
        set_state(ConnectionState::Error);
        return LomResponse{false, std::nullopt,
                           std::string{"recv payload failed"}, std::nullopt};
    }

    auto resp = LomProtocol::deserialize_response(response_data);
    if (!resp) {
        return LomResponse{false, std::nullopt,
                           std::string{"invalid response JSON"}, std::nullopt};
    }
    return *resp;
}

LomResponse TcpTransport::send(const LomRequest& request) {
    if (state_ != ConnectionState::Connected) {
        return LomResponse{false, std::nullopt,
                           std::string{"not connected"}, std::nullopt};
    }
    return send_and_receive(LomProtocol::serialize_request(request));
}

LomResponse TcpTransport::send_notes(
    const LomPath& clip_path,
    const std::vector<LomNoteData>& notes
) {
    if (state_ != ConnectionState::Connected) {
        return LomResponse{false, std::nullopt,
                           std::string{"not connected"}, std::nullopt};
    }

    // Build a call_method request for add_notes with serialised note data
    LomRequest req;
    req.type = LomRequestType::CallMethod;
    req.path = clip_path;
    req.property_or_method = "add_notes";
    req.args.push_back(LomProtocol::serialize_notes(notes));

    return send_and_receive(LomProtocol::serialize_request(req));
}

bool TcpTransport::is_connected() const {
    return state_ == ConnectionState::Connected;
}

void TcpTransport::on_state_change(
    std::function<void(ConnectionState)> callback
) {
    state_callback_ = std::move(callback);
}

}  // namespace Sunny::Infrastructure
