/**
 * @file INBR001A.cpp
 * @brief LOM Bridge Protocol implementation
 *
 * Component: INBR001A
 */

#include "INBR001A.h"

#include <sstream>

namespace Sunny::Infrastructure {

std::string LomPath::to_string() const {
    std::ostringstream oss;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) oss << "/";
        oss << segments[i];
    }
    return oss.str();
}

LomPath LomPath::parse(const std::string& path) {
    LomPath result;
    std::istringstream iss(path);
    std::string segment;
    while (std::getline(iss, segment, '/')) {
        if (!segment.empty()) {
            result.segments.push_back(segment);
        }
    }
    return result;
}

LomPath LomPath::child(const std::string& name) const {
    LomPath result = *this;
    result.segments.push_back(name);
    return result;
}

LomPath LomPath::child(int index) const {
    return child(std::to_string(index));
}

std::string LomProtocol::serialize_request(const LomRequest& request) {
    std::ostringstream oss;
    oss << "{";

    // Type
    oss << "\"type\":\"";
    switch (request.type) {
        case LomRequestType::GetProperty: oss << "get"; break;
        case LomRequestType::SetProperty: oss << "set"; break;
        case LomRequestType::CallMethod: oss << "call"; break;
        case LomRequestType::Observe: oss << "observe"; break;
        case LomRequestType::Unobserve: oss << "unobserve"; break;
    }
    oss << "\",";

    // Path
    oss << "\"path\":\"" << request.path.to_string() << "\",";

    // Property or method
    oss << "\"name\":\"" << request.property_or_method << "\"";

    // Args (simplified serialization)
    if (!request.args.empty()) {
        oss << ",\"args\":[";
        for (std::size_t i = 0; i < request.args.size(); ++i) {
            if (i > 0) oss << ",";
            std::visit([&oss](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, bool>) {
                    oss << (v ? "true" : "false");
                } else if constexpr (std::is_same_v<T, int>) {
                    oss << v;
                } else if constexpr (std::is_same_v<T, double>) {
                    oss << v;
                } else if constexpr (std::is_same_v<T, std::string>) {
                    oss << "\"" << v << "\"";
                } else {
                    oss << "null";  // Arrays not fully supported
                }
            }, request.args[i]);
        }
        oss << "]";
    }

    // Callback ID
    if (request.callback_id) {
        oss << ",\"callback_id\":\"" << *request.callback_id << "\"";
    }

    oss << "}";
    return oss.str();
}

std::optional<LomResponse> LomProtocol::deserialize_response(
    const std::string& /* json */
) {
    // Simplified - actual implementation would use JSON parser
    LomResponse response;
    response.success = true;
    return response;
}

std::string LomProtocol::serialize_notes(const std::vector<LomNoteData>& notes) {
    std::ostringstream oss;
    oss << "[";
    for (std::size_t i = 0; i < notes.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& n = notes[i];
        oss << "{\"pitch\":" << static_cast<int>(n.pitch)
            << ",\"start\":" << n.start_time
            << ",\"duration\":" << n.duration
            << ",\"velocity\":" << static_cast<int>(n.velocity)
            << ",\"muted\":" << (n.muted ? "true" : "false")
            << "}";
    }
    oss << "]";
    return oss.str();
}

LomNoteData LomProtocol::from_note_event(const Core::NoteEvent& event) {
    LomNoteData data;
    data.pitch = event.pitch;
    data.start_time = event.start_time.to_float();
    data.duration = event.duration.to_float();
    data.velocity = event.velocity;
    data.muted = false;
    return data;
}

LomRequest LomProtocol::get_property(
    const LomPath& path,
    const std::string& property
) {
    LomRequest req;
    req.type = LomRequestType::GetProperty;
    req.path = path;
    req.property_or_method = property;
    return req;
}

LomRequest LomProtocol::set_property(
    const LomPath& path,
    const std::string& property,
    const LomValue& value
) {
    LomRequest req;
    req.type = LomRequestType::SetProperty;
    req.path = path;
    req.property_or_method = property;
    req.args.push_back(value);
    return req;
}

LomRequest LomProtocol::call_method(
    const LomPath& path,
    const std::string& method,
    const std::vector<LomValue>& args
) {
    LomRequest req;
    req.type = LomRequestType::CallMethod;
    req.path = path;
    req.property_or_method = method;
    req.args = args;
    return req;
}

}  // namespace Sunny::Infrastructure
