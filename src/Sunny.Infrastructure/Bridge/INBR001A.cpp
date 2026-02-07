/**
 * @file INBR001A.cpp
 * @brief LOM Bridge Protocol implementation
 *
 * Component: INBR001A
 *
 * Serialisation uses nlohmann/json for correctness (proper string
 * escaping, numeric precision). Deserialisation parses JSON responses
 * from the Python Remote Script.
 */

#include "INBR001A.h"

#include <nlohmann/json.hpp>
#include <sstream>

namespace Sunny::Infrastructure {

using json = nlohmann::json;

// =============================================================================
// LomPath
// =============================================================================

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

// =============================================================================
// Serialisation
// =============================================================================

namespace {

json lom_value_to_json(const LomValue& value) {
    return std::visit([](const auto& v) -> json {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
            return v;
        } else if constexpr (std::is_same_v<T, int>) {
            return v;
        } else if constexpr (std::is_same_v<T, double>) {
            return v;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else if constexpr (std::is_same_v<T, std::vector<int>>) {
            return json(v);
        } else if constexpr (std::is_same_v<T, std::vector<double>>) {
            return json(v);
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            return json(v);
        } else {
            return nullptr;
        }
    }, value);
}

LomValue json_to_lom_value(const json& j) {
    if (j.is_boolean()) {
        return j.get<bool>();
    } else if (j.is_number_integer()) {
        return j.get<int>();
    } else if (j.is_number_float()) {
        return j.get<double>();
    } else if (j.is_string()) {
        return j.get<std::string>();
    } else if (j.is_array() && !j.empty()) {
        if (j[0].is_number_integer()) {
            return j.get<std::vector<int>>();
        } else if (j[0].is_number_float()) {
            return j.get<std::vector<double>>();
        } else if (j[0].is_string()) {
            return j.get<std::vector<std::string>>();
        }
    }
    return std::string{};  // Fallback
}

}  // namespace

std::string LomProtocol::serialize_request(const LomRequest& request) {
    json j;

    switch (request.type) {
        case LomRequestType::GetProperty: j["type"] = "get"; break;
        case LomRequestType::SetProperty: j["type"] = "set"; break;
        case LomRequestType::CallMethod:  j["type"] = "call"; break;
        case LomRequestType::Observe:     j["type"] = "observe"; break;
        case LomRequestType::Unobserve:   j["type"] = "unobserve"; break;
    }

    j["path"] = request.path.to_string();
    j["name"] = request.property_or_method;

    if (!request.args.empty()) {
        json args_array = json::array();
        for (const auto& arg : request.args) {
            args_array.push_back(lom_value_to_json(arg));
        }
        j["args"] = args_array;
    }

    if (request.callback_id) {
        j["callback_id"] = *request.callback_id;
    }

    return j.dump();
}

std::optional<LomResponse> LomProtocol::deserialize_response(
    const std::string& input
) {
    json j;
    try {
        j = json::parse(input);
    } catch (const json::parse_error&) {
        return std::nullopt;
    }

    LomResponse response;

    if (j.contains("success") && j["success"].is_boolean()) {
        response.success = j["success"].get<bool>();
    } else {
        response.success = false;
    }

    if (j.contains("value") && !j["value"].is_null()) {
        response.value = json_to_lom_value(j["value"]);
    }

    if (j.contains("error") && j["error"].is_string()) {
        response.error = j["error"].get<std::string>();
    }

    if (j.contains("callback_id") && j["callback_id"].is_string()) {
        response.callback_id = j["callback_id"].get<std::string>();
    }

    return response;
}

std::string LomProtocol::serialize_notes(const std::vector<LomNoteData>& notes) {
    json arr = json::array();
    for (const auto& n : notes) {
        arr.push_back({
            {"pitch",    static_cast<int>(n.pitch)},
            {"start",    n.start_time},
            {"duration", n.duration},
            {"velocity", static_cast<int>(n.velocity)},
            {"muted",    n.muted}
        });
    }
    return arr.dump();
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

// =============================================================================
// Request builders
// =============================================================================

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
