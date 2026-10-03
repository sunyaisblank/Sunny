/**
 * @file lom_protocol.hpp
 * @brief Ableton LOM Bridge Protocol
 *
 *
 * Defines the protocol for communicating with Ableton Live
 * via the Remote Script bridge. This is a pure protocol definition;
 * actual transport is handled by LomTransport.
 */

#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <sunny/core/types/music_types.hpp>
#include <sunny/core/types/note_event.hpp>
#include <variant>
#include <vector>

namespace sunny::infrastructure {

/// LOM path segments
struct LomPath {
    std::vector<std::string> segments;

    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] static LomPath parse(const std::string& path);
    [[nodiscard]] bool is_canonical() const;
    [[nodiscard]] LomPath child(const std::string& name) const;
    [[nodiscard]] LomPath child(int index) const;
};

/// Value types for LOM properties
using LomValue = std::variant<bool,
                              int,
                              double,
                              std::string,
                              std::vector<int>,
                              std::vector<double>,
                              std::vector<std::string>,
                              nlohmann::json>;

/// Request types implemented end-to-end by both native and Live peers.
enum class LomRequestType { GetProperty, SetProperty, CallMethod };

/// What the native transport can prove about delivery of a request frame.
enum class LomDeliveryState : std::uint8_t {
    NotSent,
    SentWithoutValidResponse,
    ResponseReceived,
};

/// Current bridge request. The serializer adds the authoritative envelope version;
/// the Live peer admits only the closed operation set defined in
/// remote_script/Sunny/bridge_contract.json.
struct LomRequest {
    LomRequestType type;
    LomPath path;
    std::string property_or_method;
    std::vector<LomValue> args;
};

/// Current bridge response after exact envelope/version validation.
struct LomResponse {
    bool success = false;
    std::optional<LomValue> value;
    std::optional<std::string> error;
    LomDeliveryState delivery = LomDeliveryState::ResponseReceived;
};

/// Note data for clip operations
struct LomNoteData {
    sunny::core::MidiNote pitch;
    double start_time;
    double duration;
    sunny::core::Velocity velocity;
    bool muted{false};
    double probability{1.0};
    double velocity_deviation{0.0};
    double release_velocity{64.0};
};

/**
 * @brief LOM Protocol Serializer
 *
 * Serializes requests/responses for transport.
 */
class LomProtocol {
  public:
    /// Prove that a request belongs to the complete current peer algebra.
    [[nodiscard]] static sunny::core::Result<void> validate_request(const LomRequest& request);

    /// Prove the canonical Clip.add_new_notes request represented by this batch.
    [[nodiscard]] static sunny::core::Result<void>
    validate_notes(const LomPath& clip_path, const std::vector<LomNoteData>& notes);

    /// Serialize request to JSON
    [[nodiscard]] static std::string serialize_request(const LomRequest& request);

    /// Decode one exact canonical current-protocol request object.
    [[nodiscard]] static sunny::core::Result<LomRequest>
    deserialize_request(const nlohmann::json& value);

    /// Deserialize response from JSON
    [[nodiscard]] static std::optional<LomResponse> deserialize_response(const std::string& json);

    /// Serialize notes for add_notes operation
    [[nodiscard]] static std::string serialize_notes(const std::vector<LomNoteData>& notes);

    /// Build the Live 11+ Clip.add_new_notes dictionary request.
    [[nodiscard]] static LomRequest add_new_notes(const LomPath& clip_path,
                                                  const std::vector<LomNoteData>& notes);

    /// Convert whole-note-based NoteEvent timing to Live quarter-note beats.
    [[nodiscard]] static sunny::core::Result<LomNoteData>
    from_note_event(const sunny::core::NoteEvent& event);

    /// Build common requests
    [[nodiscard]] static LomRequest get_property(const LomPath& path, const std::string& property);

    [[nodiscard]] static LomRequest
    set_property(const LomPath& path, const std::string& property, const LomValue& value);

    [[nodiscard]] static LomRequest call_method(const LomPath& path,
                                                const std::string& method,
                                                const std::vector<LomValue>& args = {});
};

/// Common LOM paths
namespace LomPaths {
[[nodiscard]] inline LomPath song() {
    return LomPath{{"song"}};
}

[[nodiscard]] inline LomPath track(int index) {
    return LomPath{{"song", "tracks", std::to_string(index)}};
}

[[nodiscard]] inline LomPath scene(int index) {
    return LomPath{{"song", "scenes", std::to_string(index)}};
}

[[nodiscard]] inline LomPath clip_slot(int track, int slot) {
    return LomPath{{"song", "tracks", std::to_string(track), "clip_slots", std::to_string(slot)}};
}

[[nodiscard]] inline LomPath clip(int track, int slot) {
    return LomPath{
        {"song", "tracks", std::to_string(track), "clip_slots", std::to_string(slot), "clip"}};
}
} // namespace LomPaths

} // namespace sunny::infrastructure
