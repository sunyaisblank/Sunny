/**
 * @file INBR002A.cpp
 * @brief Bridge Message Dispatcher implementation
 *
 * Component: INBR002A
 */

#include "INBR002A.h"

namespace Sunny::Infrastructure {

namespace {

/// Convert orchestrator note events to wire note data
std::vector<LomNoteData> to_note_data(const std::vector<Core::NoteEvent>& notes) {
    std::vector<LomNoteData> data;
    data.reserve(notes.size());
    for (const auto& event : notes) {
        data.push_back(LomProtocol::from_note_event(event));
    }
    return data;
}

}  // namespace

LomRequest BridgeDispatcher::to_lom_request(const BridgeMessage& msg) {
    const LomPath path = LomPath::parse(msg.path);

    switch (msg.type) {
        case BridgeMessageType::CreateClip: {
            // args[0] carries the clip length in beats
            std::vector<LomValue> args;
            if (!msg.args.empty()) {
                args.emplace_back(std::stod(msg.args[0]));
            }
            return LomProtocol::call_method(path, "create_clip", args);
        }

        case BridgeMessageType::CallMethod: {
            // args[0] is the method name; the rest are string arguments
            std::string method = msg.args.empty() ? std::string{} : msg.args[0];
            std::vector<LomValue> args;
            for (std::size_t i = 1; i < msg.args.size(); ++i) {
                args.emplace_back(msg.args[i]);
            }
            return LomProtocol::call_method(path, method, args);
        }

        case BridgeMessageType::SetProperty: {
            // args[0] is the property name, args[1] its value
            std::string property = msg.args.empty() ? std::string{} : msg.args[0];
            LomValue value = msg.args.size() > 1 ? LomValue{msg.args[1]}
                                                 : LomValue{std::string{}};
            return LomProtocol::set_property(path, property, value);
        }

        case BridgeMessageType::GetProperty: {
            std::string property = msg.args.empty() ? std::string{} : msg.args[0];
            return LomProtocol::get_property(path, property);
        }

        case BridgeMessageType::AddNotes:
        case BridgeMessageType::Batch:
            break;
    }

    // AddNotes is routed through send_notes in dispatch(); Batch is not
    // produced by the Orchestrator. Surface either as an empty CallMethod
    // so the Remote Script rejects it visibly rather than acting on a guess.
    return LomProtocol::call_method(path, "");
}

DispatchReport BridgeDispatcher::dispatch(
    const std::vector<BridgeMessage>& messages
) {
    DispatchReport report;

    if (!online()) {
        report.failed = messages.size();
        report.errors.emplace_back("transport offline: no Ableton connection");
        return report;
    }

    for (const auto& msg : messages) {
        LomResponse response;
        if (msg.type == BridgeMessageType::AddNotes) {
            response = transport_->send_notes(
                LomPath::parse(msg.path), to_note_data(msg.notes));
        } else {
            response = transport_->send(to_lom_request(msg));
        }

        if (response.success) {
            ++report.sent;
        } else {
            ++report.failed;
            report.errors.push_back(
                msg.path + ": " + response.error.value_or("unknown error"));
        }
    }

    return report;
}

}  // namespace Sunny::Infrastructure
