/** Bounded configuration parsing and explicit, exclusive legacy migration. */
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <sunny/infrastructure/runtime_configuration.hpp>
#include <sys/stat.h>
#include <system_error>
#include <vector>
#ifdef _WIN32
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace sunny::infrastructure {
namespace {
using json = nlohmann::json;
constexpr std::array legacy_keys{"SUNNY_ABLETON_HOST",
                                 "SUNNY_TCP_PORT",
                                 "SUNNY_WORKSPACE_PATH",
                                 "SUNNY_WORKSPACE_RECOVERY",
                                 "SUNNY_BIND_HOST"};
[[noreturn]] void invalid(const std::string& message) {
    throw std::runtime_error(message);
}
void fields(const json& value,
            std::initializer_list<const char*> required,
            std::initializer_list<const char*> optional = {},
            const char* label = "configuration") {
    if (!value.is_object()) invalid(std::string(label) + " must be an object");
    for (const auto* key : required)
        if (!value.contains(key)) invalid(std::string(label) + " has missing fields");
    for (const auto& [key, item] : value.items()) {
        static_cast<void>(item);
        const auto has = [&](auto keys) {
            return std::any_of(
                keys.begin(), keys.end(), [&](const char* entry) { return key == entry; });
        };
        if (!has(required) && !has(optional)) invalid(std::string(label) + " has unknown fields");
    }
}
std::string text(const json& value, std::size_t limit, const char* label) {
    if (!value.is_string()) invalid(std::string(label) + " must be a nonempty string");
    const auto result = value.get<std::string>();
    if (result.empty()) invalid(std::string(label) + " must be a nonempty string");
    if (result.size() > limit ||
        std::any_of(result.begin(), result.end(), [](unsigned char character) {
            return character < 32 || character == 127;
        }))
        invalid(std::string(label) + " contains invalid characters or exceeds its byte limit");
    // Environment strings need the same UTF-8 validation as parsed JSON strings.
    try {
        static_cast<void>(json(result).dump());
    } catch (const json::exception&) {
        invalid(std::string(label) + " must be UTF-8");
    }
    return result;
}
bool ascii_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
bool ascii_digit(char c) {
    return c >= '0' && c <= '9';
}
std::string path_text(const json& value) {
    auto result = text(value, SUNNY_CONFIGURATION_MAX_PATH_BYTES, "client.workspace.path");
    const bool drive = result.size() >= 3 && ascii_letter(result[0]) && result[1] == ':' &&
                       (result[2] == '/' || result[2] == '\\');
    if (result.front() != '/' && !result.starts_with("\\\\") && !drive)
        invalid("client.workspace.path must be absolute on its consumer platform");
    return result;
}
std::string host_text(const json& value, const char* label, bool native = false) {
    auto result = text(value, 253, label);
    in_addr ipv4{};
    in6_addr ipv6{};
    const bool is_v4 = inet_pton(AF_INET, result.c_str(), &ipv4) == 1;
    const bool is_v6 = inet_pton(AF_INET6, result.c_str(), &ipv6) == 1;
    if (native) {
        if (!is_v4 || (ntohl(ipv4.s_addr) >> 24) != 127)
            invalid(std::string(label) + " must be an explicit IPv4 loopback address");
        return result;
    }
    if (is_v4 || is_v6) return result;
    if (std::all_of(
            result.begin(), result.end(), [](char c) { return ascii_digit(c) || c == '.'; }))
        invalid(std::string(label) + " must be an ASCII DNS name or IPv4/IPv6 address");
    const std::string_view name(result.data(), result.size() - (result.back() == '.' ? 1U : 0U));
    std::size_t start = 0;
    while (start < name.size()) {
        const auto end = name.find('.', start);
        const auto part = name.substr(start, end == std::string_view::npos ? end : end - start);
        const auto alnum = [](char c) { return ascii_letter(c) || ascii_digit(c); };
        if (part.empty() || part.size() > 63 || !alnum(part.front()) || !alnum(part.back()) ||
            !std::all_of(part.begin(), part.end(), [&](char c) { return alnum(c) || c == '-'; }))
            invalid(std::string(label) + " must be an ASCII DNS name or IPv4/IPv6 address");
        if (end == std::string_view::npos) return result;
        start = end + 1;
        if (start == name.size()) invalid(std::string(label) + " has an empty DNS label");
    }
    invalid(std::string(label) + " must be an ASCII DNS name or IPv4/IPv6 address");
}
std::uint16_t port(const json& value, const char* label) {
    if (!value.is_number_integer() ||
        (value.is_number_unsigned() && value.get<std::uint64_t>() > 65535))
        invalid(std::string(label) + " must be an integer from 1 to 65535");
    const auto number = value.get<std::int64_t>();
    if (number < 1 || number > 65535)
        invalid(std::string(label) + " must be an integer from 1 to 65535");
    return static_cast<std::uint16_t>(number);
}
void validate(const json& document) {
    fields(document, {"configuration_schema_version"}, {"client", "native"});
    const auto& version = document["configuration_schema_version"];
    if (!version.is_number_integer() || version != SUNNY_CONFIGURATION_SCHEMA_VERSION)
        invalid("Unsupported configuration_schema_version; preserve the source and use a supported "
                "migrator");
    if (!document.contains("client") && !document.contains("native"))
        invalid("Configuration requires a client or native role");
    if (document.contains("client")) {
        const auto& client = document["client"];
        fields(client, {"transport", "workspace"}, {}, "client");
        const auto& transport = client["transport"];
        if (transport.is_object() && transport.value("mode", json{}) == "offline")
            fields(transport, {"mode"}, {}, "client.transport");
        else {
            fields(transport, {"mode", "host", "port"}, {}, "client.transport");
            if (transport["mode"] != "tcp")
                invalid("client.transport.mode must be 'offline' or 'tcp'");
            static_cast<void>(host_text(transport["host"], "client.transport.host"));
            static_cast<void>(port(transport["port"], "client.transport.port"));
        }
        const auto& workspace = client["workspace"];
        if (!workspace.is_null()) {
            fields(workspace, {"path", "recovery"}, {}, "client.workspace");
            static_cast<void>(path_text(workspace["path"]));
            if (workspace["recovery"] != "none" && workspace["recovery"] != "backup")
                invalid("client.workspace.recovery must be 'none' or 'backup'");
        }
    }
    if (document.contains("native")) {
        fields(document["native"], {"bridge"}, {}, "native");
        const auto& bridge = document["native"]["bridge"];
        fields(bridge, {"bind_host", "port"}, {}, "native.bridge");
        static_cast<void>(host_text(bridge["bind_host"], "native.bridge.bind_host", true));
        static_cast<void>(port(bridge["port"], "native.bridge.port"));
    }
}
json parse(std::string_view bytes) {
    if (bytes.size() > SUNNY_CONFIGURATION_MAX_BYTES) invalid("Configuration exceeds 64 KiB");
    // Count containers before parsing: a scalar child at depth 8 remains permitted.
    int nesting = 0;
    bool quoted = false;
    bool escaped = false;
    for (char c : bytes) {
        if (quoted) {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                quoted = false;
        } else if (c == '"')
            quoted = true;
        else if (c == '[' || c == '{') {
            if (++nesting > SUNNY_CONFIGURATION_MAX_DEPTH) invalid("Configuration exceeds depth 8");
        } else if (c == ']' || c == '}')
            --nesting;
    }
    std::vector<std::set<std::string>> objects;
    json result;
    try {
        result = json::parse(bytes, [&](int, json::parse_event_t event, json& value) {
            if (event == json::parse_event_t::object_start)
                objects.emplace_back();
            else if (event == json::parse_event_t::key) {
                if (objects.empty() || !objects.back().insert(value.get<std::string>()).second)
                    invalid("Configuration contains duplicate JSON fields");
            } else if (event == json::parse_event_t::object_end)
                objects.pop_back();
            return true;
        });
    } catch (const json::exception&) {
        invalid("Configuration must be valid finite UTF-8 JSON");
    }
    validate(result);
    return result;
}
std::filesystem::path utf8_path(std::string_view value) {
    return std::filesystem::path(std::u8string(value.begin(), value.end()));
}
std::string read_file(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path))
        invalid("Selected configuration must be a regular file");
    std::ifstream source(path, std::ios::binary);
    if (!source) invalid("Cannot read selected configuration file");
    std::string bytes(SUNNY_CONFIGURATION_MAX_BYTES + 1, '\0');
    source.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<std::size_t>(source.gcount()));
    if (source.bad()) invalid("Cannot read selected configuration file");
    return bytes;
}
ClientRuntimeConfiguration client(const json& document) {
    if (!document.contains("client"))
        invalid("Configuration requires the client role for sunny-mcp");
    ClientRuntimeConfiguration result;
    result.legacy_environment = false;
    const auto& role = document["client"];
    if (role["transport"]["mode"] == "tcp") {
        TcpConfig tcp;
        tcp.host = role["transport"]["host"].get<std::string>();
        tcp.port = port(role["transport"]["port"], "client.transport.port");
        result.transport = std::move(tcp);
    }
    if (!role["workspace"].is_null()) {
        const auto path = utf8_path(role["workspace"]["path"].get<std::string>());
        if (!path.is_absolute())
            invalid("client.workspace.path must be absolute on this consumer platform");
        result.workspace =
            WorkspaceStartupConfiguration{path, role["workspace"]["recovery"] == "backup"};
    }
    return result;
}
std::uint16_t legacy_port(const RuntimeEnvironment& environment) {
    const auto found = environment.find("SUNNY_TCP_PORT");
    if (found == environment.end()) return 9001;
    const auto& raw = found->second;
    unsigned parsed = 0;
    const auto [end, error] = std::from_chars(raw.data(), raw.data() + raw.size(), parsed);
    if (raw.empty() || raw.front() < '1' || raw.front() > '9' || raw.size() > 5 ||
        error != std::errc{} || end != raw.data() + raw.size() || parsed < 1 || parsed > 65535)
        invalid("SUNNY_TCP_PORT must be an ASCII decimal port from 1 to 65535 without signs, "
                "whitespace, or leading zeros");
    return static_cast<std::uint16_t>(parsed);
}
void exclusive_write(const std::filesystem::path& output, const std::string& body) {
#ifdef _WIN32
    const int descriptor =
        _wopen(output.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
    const int descriptor = ::open(output.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
#endif
    if (descriptor < 0)
        throw std::system_error(
            errno, std::generic_category(), "Cannot create exclusive migration output");
    int saved_error = 0;
    std::size_t offset = 0;
    while (offset < body.size()) {
#ifdef _WIN32
        const auto count =
            _write(descriptor, body.data() + offset, static_cast<unsigned>(body.size() - offset));
#else
        const auto count = ::write(descriptor, body.data() + offset, body.size() - offset);
#endif
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            saved_error = count < 0 ? errno : EIO;
            break;
        }
        offset += static_cast<std::size_t>(count);
    }
#ifdef _WIN32
    if (!saved_error && _commit(descriptor) != 0) saved_error = errno;
    if (_close(descriptor) != 0 && !saved_error) saved_error = errno;
#else
    if (!saved_error && ::fsync(descriptor) != 0) saved_error = errno;
    if (::close(descriptor) != 0 && !saved_error) saved_error = errno;
#endif
    if (saved_error)
        throw std::system_error(saved_error,
                                std::generic_category(),
                                "Migration output incomplete; preserve and inspect it");
}
} // namespace

RuntimeEnvironment runtime_environment() {
    RuntimeEnvironment result;
    for (const auto* key : legacy_keys)
        if (const auto* value = std::getenv(key))
            result.emplace(key, *value ? text(value, SUNNY_CONFIGURATION_MAX_PATH_BYTES, key) : "");
    if (const auto* selected = std::getenv("SUNNY_CONFIG_PATH"))
        result.emplace("SUNNY_CONFIG_PATH",
                       text(selected, SUNNY_CONFIGURATION_MAX_PATH_BYTES, "SUNNY_CONFIG_PATH"));
    return result;
}
void validate_runtime_configuration(std::string_view bytes) {
    static_cast<void>(parse(bytes));
}
std::string validate_client_configuration_file(const std::filesystem::path& path) {
    const auto document = parse(read_file(path));
    static_cast<void>(client(document));
    return document.dump();
}
ClientRuntimeConfiguration load_client_configuration(const RuntimeEnvironment& environment) {
    if (const auto selected = environment.find("SUNNY_CONFIG_PATH");
        selected != environment.end()) {
        for (const auto* key : legacy_keys)
            if (environment.contains(key))
                invalid("SUNNY_CONFIG_PATH cannot be combined with legacy Sunny runtime variables");
        const auto path =
            text(selected->second, SUNNY_CONFIGURATION_MAX_PATH_BYTES, "SUNNY_CONFIG_PATH");
        return client(parse(read_file(utf8_path(path))));
    }
    ClientRuntimeConfiguration result;
    const auto port_number = legacy_port(environment);
    if (const auto host = environment.find("SUNNY_ABLETON_HOST"); host != environment.end()) {
        if (host->second.empty()) invalid("SUNNY_ABLETON_HOST must not be empty");
        TcpConfig tcp;
        tcp.host = text(host->second, 253, "SUNNY_ABLETON_HOST");
        tcp.port = port_number;
        result.transport = std::move(tcp);
    }
    const auto recovery = environment.find("SUNNY_WORKSPACE_RECOVERY");
    if (recovery != environment.end() && recovery->second != "backup")
        invalid("SUNNY_WORKSPACE_RECOVERY must be 'backup' or unset");
    if (const auto path = environment.find("SUNNY_WORKSPACE_PATH"); path != environment.end()) {
        if (path->second.empty()) invalid("SUNNY_WORKSPACE_PATH must not be empty");
        const auto selected =
            text(path->second, SUNNY_CONFIGURATION_MAX_PATH_BYTES, "SUNNY_WORKSPACE_PATH");
        result.workspace = WorkspaceStartupConfiguration{
            std::filesystem::absolute(utf8_path(selected)), recovery != environment.end()};
    } else if (recovery != environment.end())
        invalid("SUNNY_WORKSPACE_RECOVERY requires SUNNY_WORKSPACE_PATH");
    return result;
}
void migrate_client_environment(const RuntimeEnvironment& environment,
                                const std::filesystem::path& output) {
    if (environment.contains("SUNNY_CONFIG_PATH"))
        invalid("Legacy migration requires SUNNY_CONFIG_PATH to be unset");
    const auto configuration = load_client_configuration(environment);
    json role{{"transport", {{"mode", "offline"}}}, {"workspace", nullptr}};
    if (configuration.transport)
        role["transport"] = {{"mode", "tcp"},
                             {"host", configuration.transport->host},
                             {"port", configuration.transport->port}};
    if (configuration.workspace) {
        const auto encoded = configuration.workspace->path.u8string();
        role["workspace"] = {
            {"path", std::string(encoded.begin(), encoded.end())},
            {"recovery", configuration.workspace->recover_backup ? "backup" : "none"}};
    }
    json document{{"configuration_schema_version", SUNNY_CONFIGURATION_SCHEMA_VERSION},
                  {"client", std::move(role)}};
    validate(document);
    exclusive_write(output, document.dump(2) + "\n");
}
} // namespace sunny::infrastructure
