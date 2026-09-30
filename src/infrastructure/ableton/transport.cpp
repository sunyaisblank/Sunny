/**
 * @file transport.cpp
 * @brief LOM Bridge Transport Layer implementation
 *
 *
 * CommandBuffer: recording transport (test double and offline compilation).
 * TcpTransport: live POSIX-socket connection to the Sunny Remote Script
 *                 TCP server (4-byte big-endian length prefix + JSON).
 */

#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <netinet/in.h>
#include <poll.h>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sys/socket.h>
#include <type_traits>
#include <unistd.h>
#include <utility>

namespace sunny::infrastructure {

namespace {

nlohmann::json modeled_devices(std::uint32_t count) {
    auto devices = nlohmann::json::array();
    for (std::uint32_t index = 0; index < count; ++index) {
        devices.push_back({{"name", "modeled-device-" + std::to_string(index)},
                           {"class_display_name", "Modeled Device"},
                           {"class_name", "ModeledDevice"},
                           {"type", 0},
                           {"is_active", true},
                           {"can_have_chains", false},
                           {"latency_in_samples", 0},
                           {"latency_in_ms", 0.0}});
    }
    return devices;
}

nlohmann::json modeled_parameter(bool quantized = false) {
    return {{"value", 0.0},
            {"display_value", 0.0},
            {"minimum", -1.0},
            {"maximum", 1.0},
            {"is_quantized", quantized},
            {"default_value", quantized ? nlohmann::json(nullptr) : nlohmann::json(0.0)},
            {"value_items", quantized ? nlohmann::json::array() : nlohmann::json(nullptr)},
            {"state", 0},
            {"automation_state", 0},
            {"is_enabled", true}};
}

nlohmann::json modeled_mixer(bool crossfade_assign_available) {
    return {{"volume", modeled_parameter()},
            {"track_activator", modeled_parameter(true)},
            {"panning", modeled_parameter()},
            {"sends", nlohmann::json::array()},
            {"crossfade_assign",
             crossfade_assign_available ? nlohmann::json(1) : nlohmann::json(nullptr)},
            {"panning_mode", 0}};
}

nlohmann::json modeled_routing(std::string display_name, std::string identifier) {
    return {{"display_name", std::move(display_name)}, {"identifier", std::move(identifier)}};
}

nlohmann::json modeled_track_like(const std::string& name,
                                  std::uint32_t device_count,
                                  bool crossfade_assign_available) {
    return {{"name", name},
            {"devices", modeled_devices(device_count)},
            {"mixer", modeled_mixer(crossfade_assign_available)}};
}

nlohmann::json modeled_return_track(const std::string& name, std::uint32_t device_count) {
    const auto routing_type = modeled_routing("Main", "modeled-main");
    const auto routing_channel = modeled_routing("1/2", "modeled-main-1-2");
    return {{"name", name},
            {"devices", modeled_devices(device_count)},
            {"mixer", modeled_mixer(true)},
            {"output_routing_type", routing_type},
            {"output_routing_channel", routing_channel},
            {"available_output_routing_types",
             {{"available_output_routing_types", nlohmann::json::array({routing_type})}}},
            {"available_output_routing_channels",
             {{"available_output_routing_channels", nlohmann::json::array({routing_channel})}}},
            {"mute", false},
            {"solo", false},
            {"muted_via_solo", false}};
}

nlohmann::json modeled_scenes(std::uint32_t count) {
    auto scenes = nlohmann::json::array();
    for (std::uint32_t index = 0; index < count; ++index) {
        scenes.push_back({{"name", "modeled-scene-" + std::to_string(index)},
                          {"is_triggered", false},
                          {"tempo_enabled", false},
                          {"tempo", -1.0},
                          {"time_signature_enabled", false},
                          {"time_signature_numerator", -1},
                          {"time_signature_denominator", -1}});
    }
    return scenes;
}

AbletonTargetSnapshot empty_modeled_snapshot(const AbletonTargetProfile& profile,
                                             std::uint32_t scene_count,
                                             std::uint32_t return_count,
                                             const std::map<std::string, std::uint32_t>& counts) {
    auto returns = nlohmann::json::array();
    for (std::uint32_t index = 0; index < return_count; ++index) {
        const auto path = "song/return_tracks/" + std::to_string(index);
        const auto found = counts.find(path);
        returns.push_back(modeled_return_track("modeled-return-" + std::to_string(index),
                                               found == counts.end() ? 0 : found->second));
    }
    const auto master = counts.find("song/master_track");
    const auto scale =
        profile.live_version.at_least(12, 0, 5)
            ? nlohmann::json{{"root_note", 0},
                             {"name", "Chromatic"},
                             {"intervals",
                              nlohmann::json::array({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11})},
                             {"mode", false}}
            : nlohmann::json(nullptr);
    const auto tuning = profile.live_version.at_least(12, 1)
                            ? nlohmann::json{{"name", "12-TET"},
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
                                                                       1100.0})}}}}
                            : nlohmann::json(nullptr);
    return {SUNNY_TARGET_SNAPSHOT_SCHEMA_VERSION,
            profile,
            {{"tempo", 120.0},
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
             {"scale", scale},
             {"tuning_system", tuning},
             {"scene_count", scene_count},
             {"scenes", modeled_scenes(scene_count)},
             {"tracks", nlohmann::json::array()},
             {"return_tracks", std::move(returns)},
             {"master_track",
              modeled_track_like("Master", master == counts.end() ? 0 : master->second, false)},
             {"cue_points", nlohmann::json::array()}}};
}

void shift_indexed_counts(std::map<std::string, std::uint32_t>& counts,
                          const std::string& prefix,
                          std::uint32_t inserted_index) {
    std::map<std::string, std::uint32_t> shifted;
    for (const auto& [path, count] : counts) {
        if (!path.starts_with(prefix)) {
            shifted[path] = count;
            continue;
        }
        const auto suffix = path.substr(prefix.size());
        if (suffix.empty() || suffix.find('/') != std::string::npos ||
            !std::all_of(suffix.begin(), suffix.end(), [](unsigned char ch) {
                return std::isdigit(ch) != 0;
            })) {
            shifted[path] = count;
            continue;
        }
        const auto index = std::stoull(suffix);
        shifted[prefix + std::to_string(index >= inserted_index ? index + 1 : index)] = count;
    }
    counts = std::move(shifted);
}

} // namespace

class TcpTransport::SocketHandle {
  public:
    SocketHandle() = default;
    explicit SocketHandle(int descriptor) noexcept : descriptor_(descriptor) {}

    ~SocketHandle() { reset(); }

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;

    SocketHandle(SocketHandle&& other) noexcept : descriptor_(other.release()) {}

    SocketHandle& operator=(SocketHandle&& other) noexcept {
        if (this != &other) reset(other.release());
        return *this;
    }

    [[nodiscard]] int get() const noexcept { return descriptor_; }
    [[nodiscard]] explicit operator bool() const noexcept { return descriptor_ >= 0; }

    [[nodiscard]] int release() noexcept { return std::exchange(descriptor_, -1); }

    void reset(int descriptor = -1) noexcept {
        if (descriptor_ >= 0) ::close(descriptor_);
        descriptor_ = descriptor;
    }

  private:
    int descriptor_{-1};
};

// =============================================================================
// CommandBuffer
// =============================================================================

LomResponse CommandBuffer::send(const LomRequest& request) {
    if (!LomProtocol::validate_request(request))
        return LomResponse{false,
                           std::nullopt,
                           std::string{"request outside Sunny bridge protocol v"} +
                               std::to_string(SUNNY_BRIDGE_PROTOCOL_VERSION),
                           LomDeliveryState::NotSent};
    Entry entry;
    entry.request = request;
    entries_.push_back(std::move(entry));
    if (request.type == LomRequestType::CallMethod && request.property_or_method == "insert_device")
        ++device_counts_[request.path.to_string()];
    if (request.type == LomRequestType::CallMethod &&
        request.property_or_method == "create_midi_track" && request.args.size() == 1) {
        if (const auto* index = std::get_if<int>(&request.args.front());
            index != nullptr && *index >= 0) {
            shift_indexed_counts(
                device_counts_, "song/tracks/", static_cast<std::uint32_t>(*index));
            device_counts_["song/tracks/" + std::to_string(*index)] = 0;
        }
    }
    if (request.type == LomRequestType::CallMethod &&
        request.property_or_method == "create_return_track") {
        const auto index = return_track_count_.value_or(0);
        device_counts_["song/return_tracks/" + std::to_string(index)] = 0;
        return_track_count_ = index + 1;
    }
    if (request.type == LomRequestType::CallMethod && request.property_or_method == "create_scene")
        scene_count_ = scene_count_.value_or(0) + 1;
    return LomResponse{true, std::nullopt, std::nullopt};
}

LomResponse CommandBuffer::send_notes(const LomPath& clip_path,
                                      const std::vector<LomNoteData>& notes) {
    if (!LomProtocol::validate_notes(clip_path, notes))
        return LomResponse{false,
                           std::nullopt,
                           std::string{"request outside Sunny bridge protocol v"} +
                               std::to_string(SUNNY_BRIDGE_PROTOCOL_VERSION),
                           LomDeliveryState::NotSent};
    Entry entry;
    entry.request.type = LomRequestType::CallMethod;
    entry.request.path = clip_path;
    entry.request.property_or_method = "add_new_notes";
    entry.notes = notes;
    entries_.push_back(std::move(entry));
    return LomResponse{true, std::nullopt, std::nullopt};
}

std::vector<const CommandBuffer::Entry*> CommandBuffer::find_by_type(LomRequestType type) const {
    std::vector<const Entry*> result;
    for (const auto& e : entries_) {
        if (e.request.type == type) result.push_back(&e);
    }
    return result;
}

std::vector<const CommandBuffer::Entry*>
CommandBuffer::find_by_path_prefix(const std::string& prefix) const {
    std::vector<const Entry*> result;
    for (const auto& e : entries_) {
        auto path_str = e.request.path.to_string();
        if (path_str.substr(0, prefix.size()) == prefix) result.push_back(&e);
    }
    return result;
}

std::size_t CommandBuffer::count_type(LomRequestType type) const {
    std::size_t count = 0;
    for (const auto& e : entries_) {
        if (e.request.type == type) ++count;
    }
    return count;
}

sunny::core::Result<std::optional<AbletonTargetSnapshot>> CommandBuffer::target_snapshot() {
    if (target_snapshot_) return target_snapshot_;
    if (!target_profile_) return std::optional<AbletonTargetSnapshot>{};
    return std::optional<AbletonTargetSnapshot>{
        empty_modeled_snapshot(*target_profile_,
                               scene_count_.value_or(0),
                               return_track_count_.value_or(0),
                               device_counts_)};
}

void CommandBuffer::set_target_snapshot(AbletonTargetSnapshot snapshot) {
    target_profile_ = snapshot.target_profile;
    device_counts_.clear();
    const auto& song = snapshot.song_state;
    for (std::size_t index = 0; index < song.at("tracks").size(); ++index) {
        device_counts_["song/tracks/" + std::to_string(index)] =
            static_cast<std::uint32_t>(song.at("tracks").at(index).at("devices").size());
    }
    for (std::size_t index = 0; index < song.at("return_tracks").size(); ++index) {
        device_counts_["song/return_tracks/" + std::to_string(index)] =
            static_cast<std::uint32_t>(song.at("return_tracks").at(index).at("devices").size());
    }
    device_counts_["song/master_track"] =
        static_cast<std::uint32_t>(song.at("master_track").at("devices").size());
    scene_count_ = song.at("scene_count").get<std::uint32_t>();
    return_track_count_ = static_cast<std::uint32_t>(song.at("return_tracks").size());
    target_snapshot_ = std::move(snapshot);
}

// =============================================================================
// TcpTransport — POSIX socket implementation
// =============================================================================

TcpTransport::TcpTransport(const TcpConfig& config)
    : config_(config), socket_(std::make_unique<SocketHandle>()) {}

TcpTransport::~TcpTransport() = default;

void TcpTransport::set_state(ConnectionState new_state) {
    state_ = new_state;
    if (state_callback_) state_callback_(state_);
}

bool TcpTransport::connect() {
    if (state_ == ConnectionState::Connected) return true;

    set_state(ConnectionState::Connecting);
    socket_->reset();

    if (config_.timeout <= std::chrono::milliseconds::zero() ||
        config_.connect_timeout < std::chrono::milliseconds::zero()) {
        set_state(ConnectionState::Error);
        return false;
    }

    SocketHandle candidate{::socket(AF_INET, SOCK_STREAM, 0)};
    if (!candidate) {
        set_state(ConnectionState::Error);
        return false;
    }

    // Set send/receive timeouts
    struct timeval tv;
    tv.tv_sec = config_.timeout.count() / 1000;
    tv.tv_usec = (config_.timeout.count() % 1000) * 1000;
    if (::setsockopt(candidate.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0 ||
        ::setsockopt(candidate.get(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        set_state(ConnectionState::Error);
        return false;
    }

    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);

    if (inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr) <= 0) {
        set_state(ConnectionState::Error);
        return false;
    }

    const int original_flags = ::fcntl(candidate.get(), F_GETFL, 0);
    if (original_flags < 0 || ::fcntl(candidate.get(), F_SETFL, original_flags | O_NONBLOCK) < 0) {
        set_state(ConnectionState::Error);
        return false;
    }

    int connect_result =
        ::connect(candidate.get(), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    bool connected = connect_result == 0;
    if (connect_result < 0 && errno == EINPROGRESS) {
        pollfd descriptor{candidate.get(), POLLOUT, 0};
        const auto timeout = std::clamp<std::int64_t>(
            config_.connect_timeout.count(), 0, std::numeric_limits<int>::max());
        connect_result = ::poll(&descriptor, 1, static_cast<int>(timeout));
        if (connect_result > 0) {
            int socket_error = 0;
            socklen_t error_size = sizeof(socket_error);
            connected =
                ::getsockopt(candidate.get(), SOL_SOCKET, SO_ERROR, &socket_error, &error_size) ==
                    0 &&
                socket_error == 0;
        }
    }

    if (!connected || ::fcntl(candidate.get(), F_SETFL, original_flags) < 0) {
        set_state(ConnectionState::Error);
        return false;
    }

    socket_->reset(candidate.release());
    set_state(ConnectionState::Connected);
    return true;
}

void TcpTransport::disconnect() {
    socket_->reset();
    if (state_ != ConnectionState::Disconnected) {
        set_state(ConnectionState::Disconnected);
    }
}

bool TcpTransport::send_all(const void* data, std::size_t n) {
    const auto* ptr = static_cast<const char*>(data);
    std::size_t sent = 0;
    while (sent < n) {
        auto r = ::send(socket_->get(), ptr + sent, n - sent, MSG_NOSIGNAL);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return false;
        sent += static_cast<std::size_t>(r);
    }
    return true;
}

bool TcpTransport::recv_all(void* data, std::size_t n) {
    auto* ptr = static_cast<char*>(data);
    std::size_t received = 0;
    while (received < n) {
        auto r = ::recv(socket_->get(), ptr + received, n - received, 0);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return false;
        received += static_cast<std::size_t>(r);
    }
    return true;
}

LomResponse TcpTransport::send_and_receive(const std::string& json_payload) {
    if (json_payload.size() > SUNNY_BRIDGE_MAX_WIRE_PAYLOAD) {
        return LomResponse{
            false, std::nullopt, std::string{"request too large"}, LomDeliveryState::NotSent};
    }

    // Send: 4-byte big-endian length + payload
    std::uint32_t len = static_cast<std::uint32_t>(json_payload.size());
    std::uint32_t net_len = htonl(len);

    if (!send_all(&net_len, sizeof(net_len)) || !send_all(json_payload.data(), len)) {
        set_state(ConnectionState::Error);
        return LomResponse{false,
                           std::nullopt,
                           std::string{"send failed"},
                           LomDeliveryState::SentWithoutValidResponse};
    }

    // Receive: 4-byte big-endian length + payload
    std::uint32_t resp_net_len = 0;
    if (!recv_all(&resp_net_len, sizeof(resp_net_len))) {
        set_state(ConnectionState::Error);
        return LomResponse{false,
                           std::nullopt,
                           std::string{"recv header failed"},
                           LomDeliveryState::SentWithoutValidResponse};
    }

    std::uint32_t resp_len = ntohl(resp_net_len);
    if (resp_len > SUNNY_BRIDGE_MAX_WIRE_PAYLOAD) {
        set_state(ConnectionState::Error);
        return LomResponse{false,
                           std::nullopt,
                           std::string{"response too large"},
                           LomDeliveryState::SentWithoutValidResponse};
    }

    std::string response_data(resp_len, '\0');
    if (!recv_all(response_data.data(), resp_len)) {
        set_state(ConnectionState::Error);
        return LomResponse{false,
                           std::nullopt,
                           std::string{"recv payload failed"},
                           LomDeliveryState::SentWithoutValidResponse};
    }

    auto resp = LomProtocol::deserialize_response(response_data);
    if (!resp) {
        return LomResponse{false,
                           std::nullopt,
                           std::string{"invalid response JSON"},
                           LomDeliveryState::SentWithoutValidResponse};
    }
    return *resp;
}

LomResponse TcpTransport::send(const LomRequest& request) {
    if (!LomProtocol::validate_request(request)) {
        return LomResponse{false,
                           std::nullopt,
                           std::string{"request outside Sunny bridge protocol v"} +
                               std::to_string(SUNNY_BRIDGE_PROTOCOL_VERSION),
                           LomDeliveryState::NotSent};
    }
    if (state_ != ConnectionState::Connected) {
        return LomResponse{
            false, std::nullopt, std::string{"not connected"}, LomDeliveryState::NotSent};
    }
    return send_and_receive(LomProtocol::serialize_request(request));
}

LomResponse TcpTransport::send_notes(const LomPath& clip_path,
                                     const std::vector<LomNoteData>& notes) {
    if (!LomProtocol::validate_notes(clip_path, notes)) {
        return LomResponse{false,
                           std::nullopt,
                           std::string{"request outside Sunny bridge protocol v"} +
                               std::to_string(SUNNY_BRIDGE_PROTOCOL_VERSION),
                           LomDeliveryState::NotSent};
    }
    if (state_ != ConnectionState::Connected) {
        return LomResponse{
            false, std::nullopt, std::string{"not connected"}, LomDeliveryState::NotSent};
    }

    return send_and_receive(
        LomProtocol::serialize_request(LomProtocol::add_new_notes(clip_path, notes)));
}

bool TcpTransport::is_connected() const {
    return state_ == ConnectionState::Connected;
}

bool TcpTransport::ensure_connected() {
    return is_connected() || connect();
}

sunny::core::Result<std::optional<AbletonTargetProfile>> TcpTransport::target_profile() {
    auto response =
        send(LomProtocol::call_method(LomPaths::song(), "sunny_get_target_profile", {}));
    if (!response.success || !response.value) {
        return std::unexpected(sunny::core::ErrorCode::ProtocolError);
    }
    const auto* value = std::get_if<nlohmann::json>(&*response.value);
    if (value == nullptr) return std::unexpected(sunny::core::ErrorCode::ProtocolError);
    auto profile = target_profile_from_json(*value);
    if (!profile) return std::unexpected(profile.error());
    return std::optional<AbletonTargetProfile>{std::move(*profile)};
}

sunny::core::Result<std::optional<AbletonTargetSnapshot>> TcpTransport::target_snapshot() {
    auto response =
        send(LomProtocol::call_method(LomPaths::song(), "sunny_get_target_snapshot", {}));
    if (!response.success || !response.value)
        return std::unexpected(sunny::core::ErrorCode::ProtocolError);
    const auto* value = std::get_if<nlohmann::json>(&*response.value);
    if (value == nullptr) return std::unexpected(sunny::core::ErrorCode::ProtocolError);
    auto snapshot = target_snapshot_from_json(*value);
    if (!snapshot) return std::unexpected(snapshot.error());
    return std::optional<AbletonTargetSnapshot>{std::move(*snapshot)};
}

sunny::core::Result<std::optional<std::uint32_t>> TcpTransport::scene_count() {
    auto response = send(LomProtocol::call_method(LomPaths::song(), "sunny_get_scene_count", {}));
    if (!response.success || !response.value)
        return std::unexpected(sunny::core::ErrorCode::ProtocolError);
    const auto* count = std::get_if<int>(&*response.value);
    if (count == nullptr || *count < 0)
        return std::unexpected(sunny::core::ErrorCode::ProtocolError);
    return std::optional<std::uint32_t>{static_cast<std::uint32_t>(*count)};
}

sunny::core::Result<std::optional<std::uint32_t>> TcpTransport::return_track_count() {
    auto response =
        send(LomProtocol::call_method(LomPaths::song(), "sunny_get_return_track_count", {}));
    if (!response.success || !response.value) {
        return std::unexpected(sunny::core::ErrorCode::ProtocolError);
    }

    const auto* count = std::get_if<int>(&*response.value);
    if (count == nullptr || *count < 0) {
        return std::unexpected(sunny::core::ErrorCode::ProtocolError);
    }
    return std::optional<std::uint32_t>{static_cast<std::uint32_t>(*count)};
}

sunny::core::Result<std::optional<std::uint32_t>>
TcpTransport::device_count(const LomPath& track_path) {
    auto response = send(LomProtocol::call_method(track_path, "sunny_get_device_count", {}));
    if (!response.success || !response.value)
        return std::unexpected(sunny::core::ErrorCode::ProtocolError);
    const auto count = std::visit(
        [](const auto& value) -> std::optional<std::uint64_t> {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, int>) {
                if (value >= 0) return static_cast<std::uint64_t>(value);
            } else if constexpr (std::is_same_v<T, nlohmann::json>) {
                if (value.is_number_unsigned()) return value.template get<std::uint64_t>();
                if (value.is_number_integer() && value.template get<std::int64_t>() >= 0)
                    return static_cast<std::uint64_t>(value.template get<std::int64_t>());
            }
            return std::nullopt;
        },
        *response.value);
    if (!count || *count > std::numeric_limits<std::uint32_t>::max())
        return std::unexpected(sunny::core::ErrorCode::ProtocolError);
    return std::optional<std::uint32_t>{static_cast<std::uint32_t>(*count)};
}

void TcpTransport::on_state_change(std::function<void(ConnectionState)> callback) {
    state_callback_ = std::move(callback);
}

} // namespace sunny::infrastructure
