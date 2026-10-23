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
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#if defined(__APPLE__)
#include <crt_externs.h>
#endif
#include <fcntl.h>
#include <limits>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sunny/infrastructure/request_control.hpp>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <type_traits>
#include <unistd.h>
#include <utility>

namespace sunny::infrastructure {

namespace {

constexpr std::string_view bridge_busy_error =
    "bridge_busy: Sunny accepts one active client; close the existing client and retry";

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

std::string_view describe(ConnectFailure failure) noexcept {
    switch (failure) {
    case ConnectFailure::InvalidConfiguration:
        return "the bridge endpoint or positive timeout configuration is invalid";
    case ConnectFailure::HostUnresolved:
        return "the host name did not resolve to an address";
    case ConnectFailure::ResolverUnavailable:
        return "the owned Python 3 hostname resolver could not run or returned invalid data; "
               "install Python 3 with its socket and JSON standard library, or use a numeric "
               "address";
    case ConnectFailure::Busy:
        return "the Sunny bridge already has an active client; close that client and retry "
               "explicitly";
    case ConnectFailure::RequestRevoked:
        return "the owning request was cancelled or expired before connection completed";
    case ConnectFailure::Refused:
        return "nothing accepted the connection; check that the Sunny control surface is "
               "active in Live and that SUNNY_TCP_PORT matches its port";
    case ConnectFailure::TimedOut:
        return "host resolution or connection exceeded the shared connect timeout";
    case ConnectFailure::SocketError:
        return "a local socket could not be created or configured";
    }
    return "unknown connection failure";
}

namespace {

/// Rank failures so that, across several resolved addresses, the report names
/// the most specific obstacle rather than whichever address was tried last.
int failure_rank(ConnectFailure failure) noexcept {
    switch (failure) {
    case ConnectFailure::RequestRevoked:
        return 4;
    case ConnectFailure::TimedOut:
        return 3;
    case ConnectFailure::Refused:
        return 2;
    case ConnectFailure::Busy:
        return 2;
    case ConnectFailure::SocketError:
        return 1;
    case ConnectFailure::InvalidConfiguration:
    case ConnectFailure::HostUnresolved:
    case ConnectFailure::ResolverUnavailable:
        return 0;
    }
    return 0;
}

int poll_milliseconds(std::chrono::steady_clock::duration remaining) noexcept {
    const auto milliseconds = std::chrono::ceil<std::chrono::milliseconds>(remaining).count();
    return static_cast<int>(
        std::clamp<std::int64_t>(milliseconds, 0, std::numeric_limits<int>::max()));
}

std::chrono::steady_clock::time_point
request_bounded_deadline(std::chrono::steady_clock::time_point configured) {
    const auto control = current_request_control();
    return control ? std::min(configured, control->deadline) : configured;
}

bool request_revoked(bool read_only) {
    const auto control = current_request_control();
    return control && (control->stop_requested() ||
                       (!read_only && control->input_closed.load(std::memory_order_acquire)));
}

std::string indeterminate(std::string_view reason) {
    return "delivery indeterminate: " + std::string{reason} +
           "; the request may have executed in Live";
}

struct ResolvedAddress {
    sockaddr_storage storage{};
    socklen_t size{};
};

using AddressResult = std::expected<std::vector<ResolvedAddress>, ConnectFailure>;

/// Numeric-only libc resolution performs no hostname lookup or service lookup.
std::vector<ResolvedAddress> numeric_addresses(const TcpConfig& config) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
    addrinfo* resolved = nullptr;
    const auto service = std::to_string(config.port);
    if (::getaddrinfo(config.host.c_str(), service.c_str(), &hints, &resolved) != 0) return {};
    const std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> addresses(resolved, &::freeaddrinfo);
    std::vector<ResolvedAddress> result;
    for (const auto* address = resolved; address; address = address->ai_next) {
        if ((address->ai_family == AF_INET || address->ai_family == AF_INET6) &&
            address->ai_addrlen <= sizeof(sockaddr_storage)) {
            ResolvedAddress item;
            item.size = address->ai_addrlen;
            std::memcpy(&item.storage, address->ai_addr, item.size);
            result.push_back(item);
        }
    }
    return result;
}

/// Each hostname lookup owns one exec'ed resolver process and one capped pipe.
/// Unlike a detached DNS thread, its lifetime ends before connect() returns.
AddressResult resolve_hostname(const TcpConfig& config,
                               std::chrono::steady_clock::time_point deadline) {
    constexpr std::string_view program = R"py(import json,socket,sys
try:
    values=socket.getaddrinfo(sys.argv[1],int(sys.argv[2]),socket.AF_UNSPEC,socket.SOCK_STREAM,socket.IPPROTO_TCP)
    rows=list(dict.fromkeys((family,address[0],address[3] if family==socket.AF_INET6 else 0)
        for family,kind,protocol,name,address in values if family in (socket.AF_INET,socket.AF_INET6)))
    if not rows: sys.exit(2)
    if len(rows)>64: sys.exit(3)
    print(json.dumps(rows,separators=(',',':')))
except socket.gaierror: sys.exit(2)
except Exception: sys.exit(3)
)py";
    if (request_revoked(true)) return std::unexpected(ConnectFailure::RequestRevoked);
    if (std::chrono::steady_clock::now() >= deadline)
        return std::unexpected(ConnectFailure::TimedOut);
    int descriptors[2];
    if (::pipe(descriptors) != 0) return std::unexpected(ConnectFailure::ResolverUnavailable);
    const auto close_pipe = [&] {
        ::close(descriptors[0]);
        ::close(descriptors[1]);
    };
    if (::fcntl(descriptors[0], F_SETFD, FD_CLOEXEC) < 0 ||
        ::fcntl(descriptors[1], F_SETFD, FD_CLOEXEC) < 0 ||
        ::fcntl(descriptors[0], F_SETFL, O_NONBLOCK) < 0) {
        close_pipe();
        return std::unexpected(ConnectFailure::ResolverUnavailable);
    }
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    if (::posix_spawn_file_actions_init(&actions) != 0) {
        close_pipe();
        return std::unexpected(ConnectFailure::ResolverUnavailable);
    }
    if (::posix_spawnattr_init(&attributes) != 0) {
        ::posix_spawn_file_actions_destroy(&actions);
        close_pipe();
        return std::unexpected(ConnectFailure::ResolverUnavailable);
    }
    const bool configured =
        ::posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0) == 0 &&
        ::posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0) ==
            0 &&
        ::posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDOUT_FILENO) == 0 &&
        ::posix_spawn_file_actions_addclose(&actions, descriptors[0]) == 0 &&
        ::posix_spawn_file_actions_addclose(&actions, descriptors[1]) == 0 &&
        ::posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP) == 0 &&
        ::posix_spawnattr_setpgroup(&attributes, 0) == 0;
    const auto service = std::to_string(config.port);
    // Values are individual argv entries, never shell text. -I -S prevents
    // PYTHONPATH/site customization from changing the stdlib resolver.
    char* const arguments[] = {const_cast<char*>(config.resolver_executable.c_str()),
                               const_cast<char*>("-I"),
                               const_cast<char*>("-S"),
                               const_cast<char*>("-c"),
                               const_cast<char*>(program.data()),
                               const_cast<char*>(config.host.c_str()),
                               const_cast<char*>(service.c_str()),
                               nullptr};
    // Darwin exposes the process environment through its CRT accessor;
    // unistd.h does not declare a global environ in the Apple SDK.
#if defined(__APPLE__)
    char** const environment = *::_NSGetEnviron();
#else
    char** const environment = ::environ;
#endif
    pid_t process = -1;
    const auto spawned = configured ? ::posix_spawnp(&process,
                                                     config.resolver_executable.c_str(),
                                                     &actions,
                                                     &attributes,
                                                     arguments,
                                                     environment)
                                    : EINVAL;
    ::posix_spawnattr_destroy(&attributes);
    ::posix_spawn_file_actions_destroy(&actions);
    ::close(descriptors[1]);
    if (spawned != 0) {
        ::close(descriptors[0]);
        return std::unexpected(ConnectFailure::ResolverUnavailable);
    }
    std::string output;
    int status = 0;
    bool exited = false, eof = false;
    auto failure = ConnectFailure::ResolverUnavailable;
    while (!exited || !eof) {
        if (request_revoked(true)) {
            failure = ConnectFailure::RequestRevoked;
            break;
        }
        const auto remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero()) {
            failure = ConnectFailure::TimedOut;
            break;
        }
        pollfd descriptor{descriptors[0], POLLIN, 0};
        const auto ready = ::poll(&descriptor, 1, std::min(10, poll_milliseconds(remaining)));
        if (ready < 0 && errno != EINTR) break;
        if (ready > 0) {
            char buffer[1024];
            const auto count = ::read(descriptors[0], buffer, sizeof(buffer));
            if (count == 0)
                eof = true;
            else if (count > 0) {
                if (output.size() + static_cast<std::size_t>(count) > 16384) break;
                output.append(buffer, static_cast<std::size_t>(count));
            } else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
                break;
        }
        if (!exited) {
            const auto waited = ::waitpid(process, &status, WNOHANG);
            if (waited == process)
                exited = true;
            else if (waited < 0 && errno != EINTR)
                break;
        }
    }
    ::close(descriptors[0]);
    if (!exited || !eof) {
        // A stalled or invalid helper cannot outlive this connection attempt.
        (void)::kill(-process, SIGKILL);
        while (::waitpid(process, &status, 0) < 0 && errno == EINTR) {}
        return std::unexpected(failure);
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return std::unexpected(WIFEXITED(status) && WEXITSTATUS(status) == 2
                                   ? ConnectFailure::HostUnresolved
                                   : ConnectFailure::ResolverUnavailable);
    const auto rows = nlohmann::json::parse(output, nullptr, false);
    if (!rows.is_array() || rows.empty() || rows.size() > 64)
        return std::unexpected(ConnectFailure::ResolverUnavailable);
    std::vector<ResolvedAddress> result;
    for (const auto& row : rows) {
        if (!row.is_array() || row.size() != 3 || !row[0].is_number_integer() ||
            !row[1].is_string() || !row[2].is_number_unsigned() ||
            row[2].get<std::uint64_t>() > std::numeric_limits<std::uint32_t>::max())
            return std::unexpected(ConnectFailure::ResolverUnavailable);
        ResolvedAddress address;
        const auto text = row[1].get_ref<const std::string&>();
        if (text.empty() || text.size() >= INET6_ADDRSTRLEN || text.find('\0') != std::string::npos)
            return std::unexpected(ConnectFailure::ResolverUnavailable);
        if (row[0] == AF_INET && row[2] == 0) {
            sockaddr_in value{};
            value.sin_family = AF_INET;
            value.sin_port = htons(config.port);
            if (::inet_pton(AF_INET, text.c_str(), &value.sin_addr) != 1)
                return std::unexpected(ConnectFailure::ResolverUnavailable);
            address.size = sizeof(sockaddr_in);
            std::memcpy(&address.storage, &value, address.size);
        } else if (row[0] == AF_INET6) {
            sockaddr_in6 value{};
            value.sin6_family = AF_INET6;
            value.sin6_port = htons(config.port);
            value.sin6_scope_id = row[2].get<std::uint32_t>();
            if (::inet_pton(AF_INET6, text.c_str(), &value.sin6_addr) != 1)
                return std::unexpected(ConnectFailure::ResolverUnavailable);
            address.size = sizeof(sockaddr_in6);
            std::memcpy(&address.storage, &value, address.size);
        } else
            return std::unexpected(ConnectFailure::ResolverUnavailable);
        result.push_back(address);
    }
    return result;
}

} // namespace

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
    bridge_identity_verified_ = false;
    bridge_identity_error_.reset();

    const auto fail = [this](ConnectFailure failure) {
        last_connect_failure_ = failure;
        set_state(ConnectionState::Error);
        return false;
    };

    if (config_.response_timeout <= std::chrono::milliseconds::zero() ||
        config_.connect_timeout <= std::chrono::milliseconds::zero() || config_.port == 0 ||
        config_.host.empty() || config_.host.size() > 1024 ||
        config_.host.find('\0') != std::string::npos) {
        return fail(ConnectFailure::InvalidConfiguration);
    }
    // One deadline includes hostname resolution and every connect attempt.
    // Resolve afresh after a reconnect; numeric literals need no subprocess.
    if (request_revoked(true)) return fail(ConnectFailure::RequestRevoked);
    const Deadline deadline =
        request_bounded_deadline(std::chrono::steady_clock::now() + config_.connect_timeout);
    auto addresses = numeric_addresses(config_);
    if (addresses.empty()) {
        if (config_.resolver_executable.empty() || config_.resolver_executable.size() > 4096 ||
            config_.resolver_executable.find('\0') != std::string::npos)
            return fail(ConnectFailure::ResolverUnavailable);
        auto resolved = resolve_hostname(config_, deadline);
        if (!resolved) return fail(resolved.error());
        addresses = std::move(*resolved);
    }
    const auto attempt = [&](const ResolvedAddress& address) -> std::optional<ConnectFailure> {
        if (request_revoked(true)) return ConnectFailure::RequestRevoked;
        if (std::chrono::steady_clock::now() >= deadline) return ConnectFailure::TimedOut;
        SocketHandle candidate{::socket(address.storage.ss_family, SOCK_STREAM, IPPROTO_TCP)};
        if (!candidate) return ConnectFailure::SocketError;
        if (::fcntl(candidate.get(), F_SETFD, FD_CLOEXEC) < 0) {
            return ConnectFailure::SocketError;
        }
        const int original_flags = ::fcntl(candidate.get(), F_GETFL, 0);
        if (original_flags < 0 ||
            ::fcntl(candidate.get(), F_SETFL, original_flags | O_NONBLOCK) < 0) {
            return ConnectFailure::SocketError;
        }

        if (::connect(candidate.get(),
                      reinterpret_cast<const sockaddr*>(&address.storage),
                      address.size) < 0) {
            if (errno != EINPROGRESS) return ConnectFailure::Refused;
            pollfd descriptor{candidate.get(), POLLOUT, 0};
            int ready = 0;
            do {
                if (request_revoked(true)) return ConnectFailure::RequestRevoked;
                if (std::chrono::steady_clock::now() >= deadline) return ConnectFailure::TimedOut;
                ready = ::poll(
                    &descriptor,
                    1,
                    std::min(50, poll_milliseconds(deadline - std::chrono::steady_clock::now())));
            } while (ready == 0 || (ready < 0 && errno == EINTR));
            if (ready < 0) return ConnectFailure::SocketError;
            int socket_error = 0;
            socklen_t error_size = sizeof(socket_error);
            if (::getsockopt(candidate.get(), SOL_SOCKET, SO_ERROR, &socket_error, &error_size) !=
                0) {
                return ConnectFailure::SocketError;
            }
            if (socket_error == ETIMEDOUT) return ConnectFailure::TimedOut;
            if (socket_error != 0) return ConnectFailure::Refused;
        }

        // Keep the socket nonblocking: send and receive poll against their
        // shared exchange deadline rather than extending per-syscall timers.
        socket_->reset(candidate.release());
        return std::nullopt;
    };

    std::optional<ConnectFailure> failure;
    for (const auto& address : addresses) {
        const auto attempt_failure = attempt(address);
        if (!attempt_failure) {
            last_connect_failure_.reset();
            set_state(ConnectionState::Connected);
            return true;
        }
        if (!failure || failure_rank(*attempt_failure) > failure_rank(*failure))
            failure = attempt_failure;
        if (*attempt_failure == ConnectFailure::TimedOut ||
            *attempt_failure == ConnectFailure::RequestRevoked)
            break;
    }
    return fail(failure.value_or(ConnectFailure::HostUnresolved));
}

void TcpTransport::disconnect() {
    socket_->reset();
    bridge_identity_verified_ = false;
    bridge_identity_error_.reset();
    if (state_ != ConnectionState::Disconnected) {
        set_state(ConnectionState::Disconnected);
    }
}

void TcpTransport::abandon_connection() {
    socket_->reset();
    bridge_identity_verified_ = false;
    bridge_identity_error_.reset();
    set_state(ConnectionState::Error);
}

TcpTransport::IdlePeer TcpTransport::idle_peer_state() const {
    pollfd descriptor{socket_->get(), POLLIN, 0};
    int ready = 0;
    do {
        ready = ::poll(&descriptor, 1, 0);
    } while (ready < 0 && errno == EINTR);
    if (ready == 0) return IdlePeer::Quiet;
    if (ready < 0) return IdlePeer::Closed;
    char byte;
    const auto received = ::recv(socket_->get(), &byte, 1, MSG_PEEK | MSG_DONTWAIT);
    if (received > 0) return IdlePeer::UnsolicitedData;
    if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return IdlePeer::Quiet;
    return IdlePeer::Closed;
}

bool TcpTransport::replace_connection_if_peer_closed() {
    const auto peer = idle_peer_state();
    if (peer == IdlePeer::Quiet) return true;
    if (peer == IdlePeer::UnsolicitedData) {
        // The native listener may reject a second client before it reads any
        // request. Consume only that exact small envelope; unexpected data
        // never causes a reconnect or an ordinary request to be sent.
        const Deadline deadline =
            request_bounded_deadline(std::chrono::steady_clock::now() + config_.response_timeout);
        std::uint32_t length = 0;
        bool rejected_busy = false;
        if (recv_all(&length, sizeof(length), deadline, true) == Receipt::Complete &&
            ntohl(length) <= 1024) {
            std::string payload(ntohl(length), '\0');
            if (recv_all(payload.data(), payload.size(), deadline, true) == Receipt::Complete) {
                const auto response = LomProtocol::deserialize_response(payload);
                rejected_busy = response && !response->success && response->error &&
                                *response->error == bridge_busy_error;
            }
        }
        last_connect_failure_ = rejected_busy ? ConnectFailure::Busy : ConnectFailure::Refused;
        abandon_connection();
        return false;
    }
    disconnect();
    return connect();
}

TcpTransport::Receipt
TcpTransport::send_all(const void* data, std::size_t n, Deadline deadline, bool read_only) {
    const auto* ptr = static_cast<const char*>(data);
    std::size_t sent = 0;
    while (sent < n) {
        if (request_revoked(read_only)) return Receipt::RequestRevoked;
        const auto remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero())
            return Receipt::DeadlineExpired;
        pollfd descriptor{socket_->get(), POLLOUT, 0};
        const auto ready = ::poll(&descriptor, 1, std::min(50, poll_milliseconds(remaining)));
        if (ready < 0 && errno == EINTR) continue;
        if (ready == 0) continue;
        if (ready < 0) return Receipt::ConnectionClosed;
        if (request_revoked(read_only)) return Receipt::RequestRevoked;
        auto r = ::send(socket_->get(), ptr + sent, n - sent, MSG_NOSIGNAL);
        if (r < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (r <= 0) return Receipt::ConnectionClosed;
        sent += static_cast<std::size_t>(r);
    }
    return Receipt::Complete;
}

TcpTransport::Receipt
TcpTransport::recv_all(void* data, std::size_t n, Deadline deadline, bool read_only) {
    auto* ptr = static_cast<char*>(data);
    std::size_t received = 0;
    while (received < n) {
        if (request_revoked(read_only)) return Receipt::RequestRevoked;
        const auto remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero())
            return Receipt::DeadlineExpired;
        pollfd descriptor{socket_->get(), POLLIN, 0};
        const int ready = ::poll(&descriptor, 1, std::min(50, poll_milliseconds(remaining)));
        if (ready < 0 && errno == EINTR) continue;
        if (ready == 0) continue;
        if (ready < 0) return Receipt::ConnectionClosed;
        const auto r = ::recv(socket_->get(), ptr + received, n - received, 0);
        if (r < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (r <= 0) return Receipt::ConnectionClosed;
        received += static_cast<std::size_t>(r);
    }
    return Receipt::Complete;
}

bool TcpTransport::verify_bridge_identity() {
    if (bridge_identity_verified_) return true;
    if (bridge_identity_error_) return false;
    const auto response = send_and_receive(LomProtocol::serialize_request(LomProtocol::call_method(
                                               LomPaths::song(), "sunny_get_target_profile", {})),
                                           false,
                                           true);
    const auto fail = [this](std::string reason) {
        if (last_connect_failure_ == ConnectFailure::Busy || request_revoked(true))
            bridge_identity_error_ = std::move(reason);
        else
            bridge_identity_error_ = std::move(reason) +
                                     "; install the Sunny Remote Script folder exported from the "
                                     "same server image, then reload the control surface";
        return false;
    };
    if (!response.success || !response.value)
        return fail("Bridge identity handshake failed: " +
                    response.error.value_or("missing target profile response"));
    const auto* value = std::get_if<nlohmann::json>(&*response.value);
    if (!value || !value->is_object() || !value->contains("adapter") ||
        !value->at("adapter").is_object() || !value->at("adapter").contains("source_sha256"))
        return fail("Bridge identity is missing adapter.source_sha256 (expected " +
                    std::string(SUNNY_BRIDGE_SOURCE_SHA256) + ")");
    const auto& identity = value->at("adapter").at("source_sha256");
    if (!identity.is_string()) return fail("Bridge adapter.source_sha256 must be a SHA256 string");
    const auto& digest = identity.get_ref<const std::string&>();
    if (digest.size() != 64 || !std::all_of(digest.begin(), digest.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        }))
        return fail("Bridge adapter.source_sha256 must contain 64 lowercase hexadecimal digits");
    if (digest != SUNNY_BRIDGE_SOURCE_SHA256)
        return fail("Bridge source identity mismatch: expected " +
                    std::string(SUNNY_BRIDGE_SOURCE_SHA256) + ", observed " + digest);
    if (!target_profile_from_json(*value))
        return fail("Bridge target profile has an incompatible protocol or adapter contract");
    bridge_identity_verified_ = true;
    return true;
}

LomResponse TcpTransport::send_and_receive(const std::string& json_payload,
                                           bool require_identity,
                                           bool read_only) {
    if (request_revoked(read_only)) {
        abandon_connection();
        return LomResponse{
            false,
            std::nullopt,
            std::string{
                "owning request cancelled, expired or input closed before native frame entry"},
            LomDeliveryState::NotSent};
    }
    if (json_payload.size() > SUNNY_BRIDGE_MAX_WIRE_PAYLOAD) {
        return LomResponse{
            false, std::nullopt, std::string{"request too large"}, LomDeliveryState::NotSent};
    }
    if (!replace_connection_if_peer_closed()) {
        std::string reason{"Bridge admission or idle reconnection failed"};
        if (last_connect_failure_) {
            reason += ": ";
            reason += describe(*last_connect_failure_);
        }
        return LomResponse{false, std::nullopt, std::move(reason), LomDeliveryState::NotSent};
    }
    if (require_identity && !verify_bridge_identity()) {
        // Only the read-only handshake was sent. The caller's ordinary request
        // has not left this process, even if the handshake reply was lost.
        return LomResponse{false, std::nullopt, *bridge_identity_error_, LomDeliveryState::NotSent};
    }

    // The response deadline starts with the send, so it bounds the whole
    // exchange rather than each receive.
    const Deadline deadline =
        request_bounded_deadline(std::chrono::steady_clock::now() + config_.response_timeout);

    // Send: 4-byte big-endian length + payload
    std::uint32_t len = static_cast<std::uint32_t>(json_payload.size());
    std::uint32_t net_len = htonl(len);

    auto sent = send_all(&net_len, sizeof(net_len), deadline, read_only);
    if (sent == Receipt::Complete) sent = send_all(json_payload.data(), len, deadline, read_only);
    if (sent != Receipt::Complete) {
        // The frame never completed and the peer dispatches only complete
        // frames; closing the socket abandons the fragment, so it cannot run.
        abandon_connection();
        return LomResponse{
            false,
            std::nullopt,
            std::string{sent == Receipt::RequestRevoked
                            ? "owning request revoked before the request frame was complete"
                        : sent == Receipt::DeadlineExpired
                            ? "exchange deadline expired before the request frame was complete"
                            : "send failed before the request frame was complete"},
            LomDeliveryState::NotSent};
    }

    const auto undelivered = [this](Receipt receipt) {
        abandon_connection();
        const auto reason =
            receipt == Receipt::RequestRevoked
                ? std::string{"owning request revoked after the complete frame was sent"}
            : receipt == Receipt::DeadlineExpired
                ? std::string{"no complete response before the shared exchange/request deadline"}
                : std::string{"the connection closed before the response"};
        return LomResponse{
            false, std::nullopt, indeterminate(reason), LomDeliveryState::SentWithoutValidResponse};
    };

    // Receive: 4-byte big-endian length + payload
    std::uint32_t resp_net_len = 0;
    if (const auto receipt = recv_all(&resp_net_len, sizeof(resp_net_len), deadline, read_only);
        receipt != Receipt::Complete) {
        return undelivered(receipt);
    }

    std::uint32_t resp_len = ntohl(resp_net_len);
    if (resp_len > SUNNY_BRIDGE_MAX_WIRE_PAYLOAD) {
        abandon_connection();
        return LomResponse{false,
                           std::nullopt,
                           indeterminate("the response exceeds the bridge frame limit"),
                           LomDeliveryState::SentWithoutValidResponse};
    }

    std::string response_data(resp_len, '\0');
    if (const auto receipt = recv_all(response_data.data(), resp_len, deadline, read_only);
        receipt != Receipt::Complete) {
        return undelivered(receipt);
    }

    auto resp = LomProtocol::deserialize_response(response_data);
    if (!resp) {
        // No valid acknowledgment exists for this frame. Drop the socket so
        // an extra/late reply cannot be mistaken for a later request's result.
        abandon_connection();
        return LomResponse{false,
                           std::nullopt,
                           indeterminate("the response is not a valid bridge envelope"),
                           LomDeliveryState::SentWithoutValidResponse};
    }
    if (!resp->success && resp->error && *resp->error == bridge_busy_error) {
        last_connect_failure_ = ConnectFailure::Busy;
        abandon_connection();
    }
    return *resp;
}

LomResponse TcpTransport::send(const LomRequest& request) {
    const auto classification = LomProtocol::classify_request(request);
    if (!classification) {
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
    const bool requires_origin = classification->requires_native_origin;
    const bool drain = classification->read_only && !requires_origin;
    if (request_revoked(drain)) {
        abandon_connection();
        return LomResponse{false,
                           std::nullopt,
                           "owning request revoked before native transmission",
                           LomDeliveryState::NotSent};
    }
    if (requires_origin) {
        const auto control = current_request_control();
        if (!control || !control->native_origin || !valid_native_origin(*control->native_origin)) {
            return LomResponse{
                false,
                std::nullopt,
                "native admission requires an immutable original epoch; run doctor_ableton first",
                LomDeliveryState::NotSent};
        }
        const auto& origin = classification->native_origin;
        if (!origin) {
            return LomResponse{false,
                               std::nullopt,
                               "raw native mutation requires prepared scope authority",
                               LomDeliveryState::NotSent};
        }
        if (origin->document_token != control->native_origin->document_token ||
            (origin->bridge_instance &&
             *origin->bridge_instance != control->native_origin->bridge_instance)) {
            return LomResponse{false,
                               std::nullopt,
                               "native frame epoch differs from immutable request admission",
                               LomDeliveryState::NotSent};
        }
    }
    const bool diagnostic = request.type == LomRequestType::CallMethod &&
                            request.path.to_string() == "song" &&
                            (request.property_or_method == "sunny_get_target_profile" ||
                             request.property_or_method == "sunny_get_remote_log");
    return send_and_receive(LomProtocol::serialize_request(request), !diagnostic, drain);
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

    if (request_revoked(false)) abandon_connection();
    return LomResponse{false,
                       std::nullopt,
                       "raw native note mutation requires prepared scope authority",
                       LomDeliveryState::NotSent};
}

bool TcpTransport::is_connected() const {
    return state_ == ConnectionState::Connected;
}

bool TcpTransport::ensure_connected() {
    if (is_connected()) return replace_connection_if_peer_closed();
    return connect();
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
