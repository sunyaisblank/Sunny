/**
 * @file INBR001A.h
 * @brief Ableton LOM Bridge Protocol
 *
 * Component: INBR001A
 * Domain: IN (Infrastructure) | Category: BR (Bridge)
 *
 * Defines the protocol for communicating with Ableton Live
 * via the Remote Script bridge. This is a pure protocol definition;
 * actual transport is handled by INTP001A.
 */

#pragma once

#include "Tensor/TNTP001A.h"
#include "Tensor/TNEV001A.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Sunny::Infrastructure {

/// LOM path segments
struct LomPath {
    std::vector<std::string> segments;

    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] static LomPath parse(const std::string& path);
    [[nodiscard]] LomPath child(const std::string& name) const;
    [[nodiscard]] LomPath child(int index) const;
};

/// Value types for LOM properties
using LomValue = std::variant<
    bool,
    int,
    double,
    std::string,
    std::vector<int>,
    std::vector<double>,
    std::vector<std::string>
>;

/// Request types
enum class LomRequestType {
    GetProperty,
    SetProperty,
    CallMethod,
    Observe,
    Unobserve
};

/// LOM request
struct LomRequest {
    LomRequestType type;
    LomPath path;
    std::string property_or_method;
    std::vector<LomValue> args;
    std::optional<std::string> callback_id;
};

/// LOM response
struct LomResponse {
    bool success;
    std::optional<LomValue> value;
    std::optional<std::string> error;
    std::optional<std::string> callback_id;
};

/// Note data for clip operations
struct LomNoteData {
    Core::MidiNote pitch;
    double start_time;
    double duration;
    Core::Velocity velocity;
    bool muted{false};
};

/**
 * @brief LOM Protocol Serializer
 *
 * Serializes requests/responses for transport.
 */
class LomProtocol {
public:
    /// Serialize request to JSON
    [[nodiscard]] static std::string serialize_request(const LomRequest& request);

    /// Deserialize response from JSON
    [[nodiscard]] static std::optional<LomResponse> deserialize_response(
        const std::string& json
    );

    /// Serialize notes for add_notes operation
    [[nodiscard]] static std::string serialize_notes(
        const std::vector<LomNoteData>& notes
    );

    /// Convert NoteEvent to LomNoteData
    [[nodiscard]] static LomNoteData from_note_event(const Core::NoteEvent& event);

    /// Build common requests
    [[nodiscard]] static LomRequest get_property(
        const LomPath& path,
        const std::string& property
    );

    [[nodiscard]] static LomRequest set_property(
        const LomPath& path,
        const std::string& property,
        const LomValue& value
    );

    [[nodiscard]] static LomRequest call_method(
        const LomPath& path,
        const std::string& method,
        const std::vector<LomValue>& args = {}
    );
};

/// Common LOM paths
namespace LomPaths {
    [[nodiscard]] inline LomPath song() {
        return LomPath{{"song"}};
    }

    [[nodiscard]] inline LomPath track(int index) {
        return LomPath{{"song", "tracks", std::to_string(index)}};
    }

    [[nodiscard]] inline LomPath clip_slot(int track, int slot) {
        return LomPath{{"song", "tracks", std::to_string(track),
                        "clip_slots", std::to_string(slot)}};
    }

    [[nodiscard]] inline LomPath clip(int track, int slot) {
        return LomPath{{"song", "tracks", std::to_string(track),
                        "clip_slots", std::to_string(slot), "clip"}};
    }
}

}  // namespace Sunny::Infrastructure
