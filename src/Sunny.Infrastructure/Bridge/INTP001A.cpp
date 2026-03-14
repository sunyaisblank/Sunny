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
// TcpTransport — structural placeholder
// =============================================================================

TcpTransport::TcpTransport(const TcpConfig& config)
    : config_(config) {}

TcpTransport::~TcpTransport() {
    disconnect();
}

bool TcpTransport::connect() {
    // Placeholder: actual TCP socket creation deferred to networking integration
    state_ = ConnectionState::Error;
    if (state_callback_) state_callback_(state_);
    return false;
}

void TcpTransport::disconnect() {
    if (state_ != ConnectionState::Disconnected) {
        state_ = ConnectionState::Disconnected;
        if (state_callback_) state_callback_(state_);
    }
}

LomResponse TcpTransport::send(const LomRequest& /*request*/) {
    if (state_ != ConnectionState::Connected) {
        return LomResponse{false, std::nullopt,
                           std::string{"not connected"}, std::nullopt};
    }
    // Placeholder: serialize, send over socket, read response
    return LomResponse{false, std::nullopt,
                       std::string{"TCP transport not implemented"}, std::nullopt};
}

LomResponse TcpTransport::send_notes(
    const LomPath& /*clip_path*/,
    const std::vector<LomNoteData>& /*notes*/
) {
    if (state_ != ConnectionState::Connected) {
        return LomResponse{false, std::nullopt,
                           std::string{"not connected"}, std::nullopt};
    }
    return LomResponse{false, std::nullopt,
                       std::string{"TCP transport not implemented"}, std::nullopt};
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
