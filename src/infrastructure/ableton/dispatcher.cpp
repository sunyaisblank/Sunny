/**
 * @file dispatcher.cpp
 * @brief Bridge Message Dispatcher implementation
 *
 */

#include <charconv>
#include <cmath>
#include <string_view>
#include <sunny/infrastructure/ableton/dispatcher.hpp>

namespace sunny::infrastructure {

namespace {

/// Convert orchestrator note events to wire note data
sunny::core::Result<std::vector<LomNoteData>>
to_note_data(const std::vector<sunny::core::NoteEvent>& notes) {
    if (notes.empty()) return std::unexpected(sunny::core::ErrorCode::FormatError);
    std::vector<LomNoteData> data;
    data.reserve(notes.size());
    for (const auto& event : notes) {
        auto converted = LomProtocol::from_note_event(event);
        if (!converted) return std::unexpected(converted.error());
        data.push_back(*converted);
    }
    return data;
}

std::optional<double> parse_positive_double(std::string_view text) {
    double value = 0.0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
    if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value) ||
        value <= 0.0)
        return std::nullopt;
    return value;
}

} // namespace

sunny::core::Result<LomRequest> BridgeDispatcher::to_lom_request(const BridgeMessage& msg) {
    const LomPath path = LomPath::parse(msg.path);
    if (!path.is_canonical()) return std::unexpected(sunny::core::ErrorCode::FormatError);

    switch (msg.type) {
    case BridgeMessageType::CreateClip: {
        if (msg.args.size() != 1) return std::unexpected(sunny::core::ErrorCode::FormatError);
        auto duration = parse_positive_double(msg.args[0]);
        if (!duration) return std::unexpected(sunny::core::ErrorCode::FormatError);
        return LomProtocol::call_method(path, "create_clip", {*duration});
    }

    case BridgeMessageType::CallMethod: {
        // args[0] is the method name; the rest are string arguments
        if (msg.args.empty() || msg.args[0].empty())
            return std::unexpected(sunny::core::ErrorCode::FormatError);
        std::string method = msg.args[0];
        std::vector<LomValue> args;
        for (std::size_t i = 1; i < msg.args.size(); ++i) {
            args.emplace_back(msg.args[i]);
        }
        return LomProtocol::call_method(path, method, args);
    }

    case BridgeMessageType::SetProperty: {
        // args[0] is the property name, args[1] its value
        if (msg.args.size() != 2 || msg.args[0].empty())
            return std::unexpected(sunny::core::ErrorCode::FormatError);
        std::string property = msg.args[0];
        LomValue value = msg.args[1];
        return LomProtocol::set_property(path, property, value);
    }

    case BridgeMessageType::GetProperty: {
        if (msg.args.size() != 1 || msg.args[0].empty())
            return std::unexpected(sunny::core::ErrorCode::FormatError);
        std::string property = msg.args[0];
        return LomProtocol::get_property(path, property);
    }

    case BridgeMessageType::AddNotes:
    case BridgeMessageType::Batch:
        break;
    }

    return std::unexpected(sunny::core::ErrorCode::FormatError);
}

DispatchReport BridgeDispatcher::dispatch(const std::vector<BridgeMessage>& messages) {
    DispatchReport report;

    if (!online()) {
        report.failed = messages.size();
        report.errors.emplace_back("transport offline: no Ableton connection");
        return report;
    }

    for (const auto& msg : messages) {
        LomResponse response;
        if (msg.type == BridgeMessageType::AddNotes) {
            const auto path = LomPath::parse(msg.path);
            auto notes = to_note_data(msg.notes);
            if (!path.is_canonical() || !notes) {
                response = LomResponse{false,
                                       std::nullopt,
                                       std::string{"invalid AddNotes bridge message"},
                                       LomDeliveryState::NotSent};
            } else {
                response = transport_->send_notes(path, *notes);
            }
        } else {
            auto request = to_lom_request(msg);
            if (!request) {
                response = LomResponse{false,
                                       std::nullopt,
                                       std::string{"invalid bridge message"},
                                       LomDeliveryState::NotSent};
            } else {
                response = transport_->send(*request);
            }
        }

        if (response.success) {
            ++report.sent;
        } else {
            ++report.failed;
            report.errors.push_back(msg.path + ": " + response.error.value_or("unknown error"));
        }
    }

    return report;
}

LomResponse BridgeDispatcher::request(const LomRequest& request) {
    if (!online()) {
        return LomResponse{false,
                           std::nullopt,
                           std::string{"transport offline: no Ableton connection"},
                           LomDeliveryState::NotSent};
    }
    return transport_->send(request);
}

sunny::core::Result<std::optional<AbletonTargetProfile>> BridgeDispatcher::target_profile() {
    if (!online()) return std::unexpected(sunny::core::ErrorCode::ConnectionFailed);
    return transport_->target_profile();
}

} // namespace sunny::infrastructure
