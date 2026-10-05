/**
 * @file server.cpp
 * @brief MCP Protocol Handler implementation
 *
 */

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sunny/infrastructure/mcp/server.hpp>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace sunny::infrastructure {

namespace {

constexpr std::string_view MODERN_PROTOCOL_VERSION = "2026-07-28";
constexpr std::string_view LATEST_LEGACY_PROTOCOL_VERSION = "2025-11-25";
constexpr std::string_view SERVER_NAME = "sunny-mcp";
constexpr std::string_view SERVER_VERSION = "0.4.0";

class StreamIo final : public McpIo {
  public:
    StreamIo(std::istream& input, std::ostream& output) : input_(input), output_(output) {}
    McpReadResult read(char& byte, const std::atomic<bool>& running) override {
        if (!running.load()) return McpReadResult::Stopped;
        const bool read_byte = static_cast<bool>(input_.get(byte));
        if (!running.load()) return McpReadResult::Stopped;
        return read_byte ? McpReadResult::Byte : McpReadResult::End;
    }
    bool write(std::string_view line,
               std::chrono::steady_clock::time_point deadline,
               const std::atomic<bool>& running) override {
        if (!running.load() || std::chrono::steady_clock::now() >= deadline) return false;
        output_.write(line.data(), static_cast<std::streamsize>(line.size()));
        output_.flush();
        return static_cast<bool>(output_) && std::chrono::steady_clock::now() < deadline;
    }

  private:
    std::istream& input_;
    std::ostream& output_;
};

#ifndef _WIN32
/** The process owns stdio. Restore inherited descriptor flags on every exit. */
class StdioIo final : public McpIo {
  public:
    StdioIo() {
        input_flags_ = ::fcntl(STDIN_FILENO, F_GETFL);
        output_flags_ = ::fcntl(STDOUT_FILENO, F_GETFL);
        if (input_flags_ < 0 || output_flags_ < 0)
            throw std::runtime_error("Cannot inspect MCP stdio descriptors");
        if (::fcntl(STDIN_FILENO, F_SETFL, input_flags_ | O_NONBLOCK) < 0)
            throw std::runtime_error("Cannot bound MCP input");
        if (::fcntl(STDOUT_FILENO, F_SETFL, output_flags_ | O_NONBLOCK) < 0) {
            ::fcntl(STDIN_FILENO, F_SETFL, input_flags_);
            throw std::runtime_error("Cannot bound MCP output");
        }
        struct sigaction ignore {};
        ignore.sa_handler = SIG_IGN;
        ::sigemptyset(&ignore.sa_mask);
        if (::sigaction(SIGPIPE, &ignore, &previous_pipe_) < 0) {
            ::fcntl(STDIN_FILENO, F_SETFL, input_flags_);
            ::fcntl(STDOUT_FILENO, F_SETFL, output_flags_);
            throw std::runtime_error("Cannot contain closed MCP output");
        }
    }
    ~StdioIo() override {
        ::fcntl(STDIN_FILENO, F_SETFL, input_flags_);
        ::fcntl(STDOUT_FILENO, F_SETFL, output_flags_);
        ::sigaction(SIGPIPE, &previous_pipe_, nullptr);
    }
    McpReadResult read(char& byte, const std::atomic<bool>& running) override {
        while (running.load()) {
            if (offset_ < available_) {
                byte = buffer_[offset_++];
                return McpReadResult::Byte;
            }
            pollfd descriptor{STDIN_FILENO, POLLIN, 0};
            const auto ready = ::poll(&descriptor, 1, 50);
            if (ready < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error("MCP input poll failed");
            }
            if (ready == 0) continue;
            const auto count = ::read(STDIN_FILENO, buffer_.data(), buffer_.size());
            if (count == 0) return McpReadResult::End;
            if (count < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
                throw std::runtime_error("MCP input read failed");
            }
            available_ = static_cast<std::size_t>(count);
            offset_ = 0;
        }
        return McpReadResult::Stopped;
    }
    bool write(std::string_view line,
               std::chrono::steady_clock::time_point deadline,
               const std::atomic<bool>& running) override {
        std::size_t offset = 0;
        while (offset < line.size() && running.load()) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining.count() <= 0) return false;
            pollfd descriptor{STDOUT_FILENO, POLLOUT, 0};
            const auto ready = ::poll(
                &descriptor, 1, static_cast<int>(std::min<std::int64_t>(50, remaining.count())));
            if (ready < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            if (ready == 0) continue;
            const auto written = ::write(STDOUT_FILENO, line.data() + offset, line.size() - offset);
            if (written < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
                return false;
            }
            if (written == 0) return false;
            offset += static_cast<std::size_t>(written);
        }
        return offset == line.size();
    }

  private:
    int input_flags_ = -1, output_flags_ = -1;
    struct sigaction previous_pipe_ {};
    std::array<char, 16384> buffer_{};
    std::size_t offset_ = 0, available_ = 0;
};
#endif

constexpr std::array<std::string_view, 4> LEGACY_PROTOCOL_VERSIONS = {
    "2024-11-05", "2025-03-26", "2025-06-18", "2025-11-25"};

bool supported_legacy_version(std::string_view version) {
    return std::ranges::find(LEGACY_PROTOCOL_VERSIONS, version) != LEGACY_PROTOCOL_VERSIONS.end();
}

nlohmann::json server_info() {
    return {{"name", SERVER_NAME}, {"version", SERVER_VERSION}};
}

nlohmann::json modern_meta() {
    return {{"io.modelcontextprotocol/serverInfo", server_info()}};
}

nlohmann::json compact_property_schema(const nlohmann::json& value) {
    if (value.is_object()) return value;

    nlohmann::json property = nlohmann::json::object();
    if (!value.is_string()) return property;

    const auto description = value.get<std::string>();
    property["description"] = description;
    if (description.starts_with("integer"))
        property["type"] = "integer";
    else if (description.starts_with("number"))
        property["type"] = "number";
    else if (description.starts_with("boolean"))
        property["type"] = "boolean";
    else if (description.starts_with("string"))
        property["type"] = "string";
    else if (description.starts_with("array"))
        property["type"] = "array";
    else if (description.starts_with("object"))
        property["type"] = "object";
    return property;
}

/// The text of a compact description with every balanced {...} group removed.
std::string outside_braces(const std::string& description) {
    std::string own;
    int depth = 0;
    for (const char character : description) {
        if (character == '{') {
            ++depth;
        } else if (character == '}') {
            if (depth > 0) --depth;
        } else if (depth == 0) {
            own.push_back(character);
        }
    }
    return own;
}

nlohmann::json normalise_input_schema(const nlohmann::json& schema) {
    if (schema.is_object() && schema.contains("type")) {
        auto normalised = schema;
        if (normalised["type"] != "object") {
            throw std::invalid_argument("MCP tool input schema root type must be object");
        }
        if (!normalised.contains("properties")) normalised["properties"] = nlohmann::json::object();
        if (!normalised.contains("required")) normalised["required"] = nlohmann::json::array();
        if (!normalised["properties"].is_object() || !normalised["required"].is_array()) {
            throw std::invalid_argument("MCP tool input schema has invalid properties or required");
        }
        return normalised;
    }

    nlohmann::json properties = nlohmann::json::object();
    nlohmann::json required = nlohmann::json::array();
    if (schema.is_object()) {
        for (const auto& [name, compact] : schema.items()) {
            properties[name] = compact_property_schema(compact);
            // Only the field's own qualifiers count: "optional" inside {...}
            // describes a nested member of an object or array element.
            const std::string own =
                compact.is_string() ? outside_braces(compact.get_ref<const std::string&>()) : "";
            const bool optional = own.find("optional") != std::string::npos ||
                                  own.find("default") != std::string::npos ||
                                  own.find("provide this OR") != std::string::npos;
            if (!optional) required.push_back(name);
        }
    }
    return {{"type", "object"}, {"properties", properties}, {"required", required}};
}

/// JSON Schema 2020-12 validation section 6.1.1: an integer is any number
/// whose fractional part is zero, so 4.0 and 1e2 qualify.
bool is_integral_number(const nlohmann::json& value) {
    if (value.is_number_integer() || value.is_number_unsigned()) return true;
    if (!value.is_number_float()) return false;
    const double number = value.get<double>();
    return std::isfinite(number) && std::trunc(number) == number;
}

/// Re-encode an integral float as a JSON integer so handlers, which read
/// integers through checked_integer, see the value the schema accepted.
/// Values outside the 64-bit domains stay floats and fail the handler's
/// representability check with a precise message.
void canonicalise_integer(nlohmann::json& value) {
    if (!value.is_number_float()) return;
    const double number = value.get<double>();
    // Both bounds are powers of two and therefore exact doubles.
    constexpr double INT64_BOUND = 9223372036854775808.0;   // 2^63
    constexpr double UINT64_BOUND = 18446744073709551616.0; // 2^64
    if (number >= -INT64_BOUND && number < INT64_BOUND) {
        value = static_cast<std::int64_t>(number);
    } else if (number >= INT64_BOUND && number < UINT64_BOUND) {
        value = static_cast<std::uint64_t>(number);
    }
}

bool type_matches(const nlohmann::json& value, std::string_view type) {
    if (type == "object") return value.is_object();
    if (type == "array") return value.is_array();
    if (type == "string") return value.is_string();
    if (type == "integer") return is_integral_number(value);
    if (type == "number") return value.is_number();
    if (type == "boolean") return value.is_boolean();
    if (type == "null") return value.is_null();
    return false;
}

/// Validate value against schema, canonicalising integral numbers in place.
bool validate_schema_value(nlohmann::json& value,
                           const nlohmann::json& schema,
                           const std::string& path,
                           std::string& error) {
    if (!schema.is_object()) return true;

    if (schema.contains("type") && schema["type"].is_string()) {
        const auto type = schema["type"].get<std::string>();
        if (!type_matches(value, type)) {
            error = path + " must be of type " + type;
            return false;
        }
        if (type == "integer") canonicalise_integer(value);
    }

    if (schema.contains("enum") && schema["enum"].is_array() &&
        std::ranges::find(schema["enum"], value) == schema["enum"].end()) {
        error = path + " must be one of the advertised enum values";
        return false;
    }

    if (value.is_object()) {
        // Explicitly closed public contracts reject unadvertised input before
        // any handler or owning/native state transition.
        if (schema.contains("additionalProperties") &&
            schema["additionalProperties"].is_boolean() &&
            !schema["additionalProperties"].get<bool>()) {
            const auto properties = schema.value("properties", nlohmann::json::object());
            for (const auto& [name, child] : value.items()) {
                static_cast<void>(child);
                if (!properties.contains(name)) {
                    error = path + "." + name + " is not an advertised property";
                    return false;
                }
            }
        }
        if (schema.contains("required") && schema["required"].is_array()) {
            for (const auto& required : schema["required"]) {
                if (!required.is_string()) continue;
                const auto name = required.get<std::string>();
                if (!value.contains(name)) {
                    error = path + "." + name + " is required";
                    return false;
                }
            }
        }
        if (schema.contains("properties") && schema["properties"].is_object()) {
            for (const auto& [name, child_schema] : schema["properties"].items()) {
                if (value.contains(name) &&
                    !validate_schema_value(value[name], child_schema, path + "." + name, error)) {
                    return false;
                }
            }
        }
    }

    if (value.is_array() && schema.contains("items")) {
        for (std::size_t index = 0; index < value.size(); ++index) {
            if (!validate_schema_value(value[index],
                                       schema["items"],
                                       path + "[" + std::to_string(index) + "]",
                                       error)) {
                return false;
            }
        }
    }
    return true;
}

/// The request id when it is one MCP admits (string or integer), else null.
nlohmann::json request_id_or_null(const nlohmann::json& message) {
    if (!message.is_object() || !message.contains("id")) return nullptr;
    const auto& id = message["id"];
    if (id.is_string() || id.is_number_integer() || id.is_number_unsigned()) return id;
    return nullptr;
}

nlohmann::json tool_result(nlohmann::json result, bool is_error, bool modern) {
    nlohmann::json response = {
        {"content", nlohmann::json::array({{{"type", "text"}, {"text", result.dump()}}})},
        {"isError", is_error}};
    if (modern || result.is_object()) response["structuredContent"] = std::move(result);
    if (modern) {
        response["resultType"] = "complete";
        response["_meta"] = modern_meta();
    }
    return response;
}

} // anonymous namespace

void McpServer::register_tool(std::string name,
                              std::string description,
                              nlohmann::json input_schema,
                              McpToolHandler handler) {
    if (name.empty() || name.size() > 128 ||
        name.find_first_not_of(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-") !=
            std::string::npos) {
        throw std::invalid_argument("Invalid MCP tool name: " + name);
    }
    if (tools_.contains(name)) throw std::invalid_argument("Duplicate MCP tool name: " + name);

    ToolEntry entry;
    entry.definition.name = name;
    entry.definition.description = std::move(description);
    entry.definition.input_schema = normalise_input_schema(input_schema);
    entry.handler = std::move(handler);
    entry.document_domain = registration_domain_;
    tools_[std::move(name)] = std::move(entry);
}

void McpServer::set_tool_document_domain(const std::string& name, McpDocumentDomain domain) {
    tools_.at(name).document_domain = domain;
}

void McpServer::set_tool_executor(McpToolExecutor executor) {
    tool_executor_ = std::move(executor);
}

void McpServer::run() {
#ifndef _WIN32
    StdioIo io;
    run(io);
#else
    throw std::runtime_error("The supported Windows MCP client runs Sunny in Docker; bounded "
                             "standalone stdio requires POSIX");
#endif
}

void McpServer::run(std::istream& input, std::ostream& output) {
    StreamIo io(input, output);
    run(io);
}

void McpServer::run(McpIo& io) {
    {
        std::lock_guard control_lock(control_mutex_);
        if (running_.load() || !controls_.empty())
            throw std::logic_error("MCP session already has active requests");
        session_stopped_ = false;
        input_revoked_ = false;
        running_.store(true);
    }
    struct Entry {
        nlohmann::json message;
        std::shared_ptr<RequestControl> control;
        nlohmann::json error;
    };
    struct Group {
        bool batch;
        std::vector<Entry> entries;
    };
    struct Output {
        std::string line;
        std::chrono::steady_clock::time_point deadline;
    };
    constexpr std::size_t MAX_QUEUED_MESSAGES = 16;
    constexpr std::size_t MAX_BATCH_ENTRIES = 1024;
    constexpr std::size_t MAX_QUEUED_OUTPUTS = 32;
    std::mutex queue_mutex, output_mutex;
    std::condition_variable ready, output_ready;
    std::deque<Group> queue;
    std::deque<Output> outputs;
    std::size_t output_bytes = 0;
    bool closed = false, output_closed = false;
    std::atomic<bool> io_failed{false};
    std::thread worker, writer;

    const auto abort = [&] {
        io_failed.store(true);
        stop();
        ready.notify_all();
        output_ready.notify_all();
    };
    // The reader never writes or waits for a writer. The finite production
    // adapter makes unread stdout revoke the entire session within five seconds.
    const auto write_line = [&](const nlohmann::json& message) {
        auto line = message.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace) + "\n";
        bool overflow = false;
        {
            std::lock_guard output_lock(output_mutex);
            if (!running_.load()) return;
            overflow = outputs.size() >= MAX_QUEUED_OUTPUTS ||
                       line.size() > MCP_MAX_OUTPUT_BYTES - output_bytes;
            if (!overflow) {
                output_bytes += line.size();
                outputs.push_back(
                    {std::move(line), std::chrono::steady_clock::now() + MCP_OUTPUT_TIMEOUT});
            }
        }
        if (overflow)
            abort();
        else
            output_ready.notify_one();
    };
    // Errors join the same FIFO as admitted requests, preserving normal stream
    // response order. Saturation still rejects immediately without blocking input.
    const auto enqueue = [&](Group group) {
        bool full = false;
        {
            std::lock_guard queue_lock(queue_mutex);
            full = queue.size() >= MAX_QUEUED_MESSAGES;
            if (!full) queue.push_back(std::move(group));
        }
        if (full) {
            auto errors = nlohmann::json::array();
            for (const auto& entry : group.entries) {
                retire_request(entry.control);
                errors.push_back(entry.error.is_null()
                                     ? make_error(request_id_or_null(entry.message),
                                                  -32000,
                                                  "Request queue is full; no tool began")
                                     : entry.error);
            }
            write_line(group.batch ? errors : errors.front());
        } else {
            ready.notify_one();
        }
    };
    const auto input_error = [&](int code, const std::string& error) {
        enqueue(Group{false, {{nullptr, nullptr, make_error(nullptr, code, error)}}});
    };
    const auto finish = [&] {
        revoke_native_input();
        {
            std::lock_guard queue_lock(queue_mutex);
            closed = true;
        }
        ready.notify_all();
        if (worker.joinable()) worker.join();
        {
            std::lock_guard control_lock(control_mutex_);
            controls_.clear();
        }
        {
            std::lock_guard output_lock(output_mutex);
            output_closed = true;
        }
        output_ready.notify_all();
        if (writer.joinable()) writer.join();
        running_.store(false);
    };
    auto on_exit =
        std::unique_ptr<void, std::function<void(void*)>>(this, [&](void*) { finish(); });
    try {
        writer = std::thread([&] {
            try {
                for (;;) {
                    Output output{};
                    {
                        std::unique_lock output_lock(output_mutex);
                        output_ready.wait_for(output_lock, std::chrono::milliseconds(50), [&] {
                            return output_closed || !running_.load() || !outputs.empty();
                        });
                        if (!running_.load() || (outputs.empty() && output_closed)) return;
                        if (outputs.empty()) continue;
                        output = std::move(outputs.front());
                        outputs.pop_front();
                    }
                    if (!io.write(output.line, output.deadline, running_)) {
                        abort();
                        return;
                    }
                    {
                        std::lock_guard output_lock(output_mutex);
                        output_bytes -= output.line.size();
                    }
                }
            } catch (...) {
                abort();
            }
        });
        worker = std::thread([&] {
            try {
                for (;;) {
                    Group group{};
                    {
                        std::unique_lock queue_lock(queue_mutex);
                        ready.wait_for(queue_lock, std::chrono::milliseconds(50), [&] {
                            return closed || !running_.load() || !queue.empty();
                        });
                        if (queue.empty() && (closed || !running_.load())) return;
                        if (queue.empty()) continue;
                        group = std::move(queue.front());
                        queue.pop_front();
                    }
                    auto responses = nlohmann::json::array();
                    for (auto& entry : group.entries) {
                        RequestControlScope scope(entry.control);
                        nlohmann::json response;
                        try {
                            response = entry.error.is_null() ? process_request(entry.message)
                                                             : entry.error;
                        } catch (const std::exception&) {
                            response = make_error(
                                request_id_or_null(entry.message), -32603, "Internal error");
                        }
                        retire_request(entry.control);
                        if (!response.is_null()) responses.push_back(std::move(response));
                    }
                    if (!responses.empty()) write_line(group.batch ? responses : responses.front());
                }
            } catch (...) {
                abort();
            }
        });
        for (;;) {
            if (!running_.load()) break;
            std::string line;
            bool oversized = false, observed = false;
            char character;
            McpReadResult state = McpReadResult::Byte;
            while ((state = io.read(character, running_)) == McpReadResult::Byte) {
                observed = true;
                if (character == '\n') break;
                if (line.size() < MCP_MAX_INPUT_BYTES)
                    line.push_back(character);
                else
                    oversized = true;
            }
            // Revoke before parsing or publishing any final unterminated line.
            if (state == McpReadResult::End) revoke_native_input();
            if (state == McpReadResult::Stopped || !running_.load()) break;
            if (!observed && line.empty()) break;
            if (oversized) {
                input_error(-32600, "Request exceeds size limit");
                continue;
            }
            if (line.empty()) continue;
            nlohmann::json message;
            try {
                message = nlohmann::json::parse(line);
            } catch (const nlohmann::json::parse_error&) {
                input_error(-32700, "Parse error");
                continue;
            }
            if (cancel_notification(message)) continue;
            const bool batch = message.is_array();
            if (batch && (message.empty() || message.size() > MAX_BATCH_ENTRIES)) {
                input_error(-32600, "Invalid Request: empty or excessive batch");
                continue;
            }
            Group group{batch, {}};
            const auto add = [&](const nlohmann::json& element) {
                if (cancel_notification(element)) return;
                Entry entry{element, nullptr, nullptr};
                if (auto terminal = preflight(element)) {
                    if (terminal->is_null()) return;
                    entry.error = std::move(*terminal);
                } else {
                    try {
                        entry.control = admit_request(element);
                    } catch (const std::invalid_argument&) {
                        entry.error = make_error(request_id_or_null(element),
                                                 -32600,
                                                 "Request ID is already in progress");
                    }
                }
                group.entries.push_back(std::move(entry));
            };
            if (batch)
                for (const auto& element : message)
                    add(element);
            else
                add(message);
            if (group.entries.empty()) continue;
            enqueue(std::move(group));
        }
    } catch (...) {
        abort();
        throw;
    }
    on_exit.reset();
    if (io_failed.load()) throw std::runtime_error("MCP session could not preserve bounded I/O");
}

void McpServer::stop() {
    std::lock_guard control_lock(control_mutex_);
    session_stopped_ = true;
    running_.store(false);
    for (const auto& [id, control] : controls_) {
        static_cast<void>(id);
        control->cancelled.store(true, std::memory_order_release);
    }
}

std::shared_ptr<RequestControl> McpServer::admit_request(const nlohmann::json& message) {
    const auto id = request_id_or_null(message);
    if (id.is_null() || !message.contains("method") || !message["method"].is_string() ||
        !message.contains("jsonrpc") || message["jsonrpc"] != "2.0")
        return nullptr;
    const auto key = id.dump();
    std::lock_guard control_lock(control_mutex_);
    if (controls_.contains(key)) throw std::invalid_argument("Duplicate active request ID");
    auto control = std::make_shared<RequestControl>(key, message["method"].get<std::string>());
    control->cancelled.store(session_stopped_, std::memory_order_release);
    control->input_closed.store(input_revoked_, std::memory_order_release);
    controls_.emplace(key, control);
    return control;
}

void McpServer::retire_request(const std::shared_ptr<RequestControl>& control) {
    if (!control) return;
    std::lock_guard control_lock(control_mutex_);
    const auto found = controls_.find(control->identity);
    if (found != controls_.end() && found->second == control) controls_.erase(found);
}

bool McpServer::cancel_notification(const nlohmann::json& message) {
    if (!message.is_object() || !message.contains("jsonrpc") || message["jsonrpc"] != "2.0" ||
        !message.contains("method") || message["method"] != "notifications/cancelled" ||
        message.contains("id"))
        return false;
    if (!message.contains("params") || !message["params"].is_object()) return true;
    const auto& params = message["params"];
    if (!params.contains("requestId") ||
        !(params["requestId"].is_string() || params["requestId"].is_number_integer()) ||
        (params.contains("reason") && !params["reason"].is_string()))
        return true;
    std::lock_guard control_lock(control_mutex_);
    const auto found = controls_.find(params["requestId"].dump());
    if (found != controls_.end() && found->second->method != "initialize")
        found->second->cancelled.store(true, std::memory_order_release);
    return true;
}

void McpServer::revoke_native_input() {
    std::lock_guard control_lock(control_mutex_);
    input_revoked_ = true;
    for (const auto& [id, control] : controls_) {
        static_cast<void>(id);
        control->input_closed.store(true, std::memory_order_release);
    }
}

std::optional<nlohmann::json> McpServer::preflight(const nlohmann::json& message) {
    if (!message.is_object())
        return make_error(nullptr, -32600, "Invalid Request: message must be a JSON object");
    if (!message.contains("method") && (message.contains("result") || message.contains("error")))
        return nlohmann::json(nullptr);
    const auto error_id = request_id_or_null(message);
    if (!message.contains("jsonrpc") || message["jsonrpc"] != "2.0")
        return make_error(error_id, -32600, "Invalid Request: missing jsonrpc 2.0");
    if (!message.contains("method") || !message["method"].is_string())
        return make_error(error_id, -32600, "Invalid Request: missing method");
    if (!message.contains("id")) return nlohmann::json(nullptr);
    if (error_id.is_null())
        return make_error(nullptr, -32600, "Invalid Request: id must be a string or integer");
    return std::nullopt;
}

nlohmann::json McpServer::process_request(const nlohmann::json& message) {
    if (cancel_notification(message)) return nullptr;
    if (auto terminal = preflight(message)) return *terminal;
    const bool direct = !current_request_control();
    auto control = current_request_control();
    if (direct) {
        try {
            control = admit_request(message);
        } catch (const std::invalid_argument&) {
            return make_error(
                request_id_or_null(message), -32600, "Request ID is already in progress");
        }
    }
    auto retire = std::unique_ptr<RequestControl, std::function<void(RequestControl*)>>(
        control.get(), [this, control, direct](RequestControl*) {
            if (direct) retire_request(control);
        });
    RequestControlScope scope(control);
    std::unique_lock request_lock(request_mutex_);
    if (control && control->cancelled.load(std::memory_order_acquire)) return nullptr;
    if (control && control->expired())
        return make_error(
            request_id_or_null(message), -32000, "Request deadline expired before dispatch");

    const auto& id = message["id"];
    const auto method = message["method"].get<std::string>();
    const auto params = message.value("params", nlohmann::json::object());
    if (!params.is_object()) return make_error(id, -32602, "params must be an object");

    bool modern = false;
    if (params.contains("_meta") && params["_meta"].is_object() &&
        params["_meta"].contains("io.modelcontextprotocol/protocolVersion")) {
        const auto& version = params["_meta"]["io.modelcontextprotocol/protocolVersion"];
        if (!version.is_string() || version != MODERN_PROTOCOL_VERSION) {
            return {{"jsonrpc", "2.0"},
                    {"id", id},
                    {"error",
                     {{"code", -32022},
                      {"message", "Unsupported protocol version"},
                      {"data",
                       {{"supported", nlohmann::json::array({MODERN_PROTOCOL_VERSION})},
                        {"requested", version}}}}}};
        }
        if (!params["_meta"].contains("io.modelcontextprotocol/clientCapabilities") ||
            !params["_meta"]["io.modelcontextprotocol/clientCapabilities"].is_object()) {
            return make_error(
                id, -32602, "Modern MCP requests require clientCapabilities metadata");
        }
        modern = true;
    }

    if (method == "server/discover") return handle_discover(id);
    if (method == "initialize") return handle_initialize(id, params);
    if (method == "ping") return make_response(id, nlohmann::json::object());
    if (method == "tools/list") return handle_tools_list(id, modern);
    if (method == "tools/call") {
        auto result = handle_tools_call(id, params, modern);
        if (control && control->cancelled.load(std::memory_order_acquire)) return nullptr;
        return result;
    }
    return make_error(id, -32601, "Method not found: " + method);
}

nlohmann::json McpServer::handle_discover(const nlohmann::json& id) {
    nlohmann::json result = {
        {"resultType", "complete"},
        {"supportedVersions",
         nlohmann::json::array({MODERN_PROTOCOL_VERSION,
                                LATEST_LEGACY_PROTOCOL_VERSION,
                                "2025-06-18",
                                "2025-03-26",
                                "2024-11-05"})},
        {"capabilities", {{"tools", {{"listChanged", false}}}}},
        {"_meta", modern_meta()},
        {"instructions",
         "Sunny provides music-theory, Score, Timbre, Mix, Corpus, and Ableton tools."},
        {"ttlMs", 300000},
        {"cacheScope", "public"}};
    return make_response(id, result);
}

nlohmann::json McpServer::handle_initialize(const nlohmann::json& id,
                                            const nlohmann::json& params) {
    if (!params.contains("protocolVersion") || !params["protocolVersion"].is_string()) {
        return make_error(id, -32602, "initialize requires a protocolVersion string");
    }
    const auto requested = params["protocolVersion"].get<std::string>();
    const auto selected = supported_legacy_version(requested)
                              ? requested
                              : std::string(LATEST_LEGACY_PROTOCOL_VERSION);
    nlohmann::json result = {{"protocolVersion", selected},
                             {"capabilities", {{"tools", {{"listChanged", false}}}}},
                             {"serverInfo", server_info()}};
    return make_response(id, result);
}

nlohmann::json McpServer::handle_tools_list(const nlohmann::json& id, bool modern) {
    nlohmann::json tools_array = nlohmann::json::array();

    for (const auto& [name, entry] : tools_) {
        tools_array.push_back({{"name", entry.definition.name},
                               {"description", entry.definition.description},
                               {"inputSchema", entry.definition.input_schema}});
    }

    nlohmann::json result = {{"tools", tools_array}};
    if (modern) {
        result["resultType"] = "complete";
        result["ttlMs"] = 300000;
        result["cacheScope"] = "public";
        result["_meta"] = modern_meta();
    }
    return make_response(id, result);
}

nlohmann::json
McpServer::handle_tools_call(const nlohmann::json& id, const nlohmann::json& params, bool modern) {
    if (!params.contains("name") || !params["name"].is_string()) {
        return make_error(id, -32602, "Missing tool name");
    }

    auto tool_name = params["name"].get<std::string>();
    auto it = tools_.find(tool_name);
    if (it == tools_.end()) {
        return make_error(id, -32602, "Unknown tool: " + tool_name);
    }

    auto arguments = params.value("arguments", nlohmann::json::object());
    if (!arguments.is_object()) return make_error(id, -32602, "Tool arguments must be an object");

    std::string validation_error;
    if (!validate_schema_value(
            arguments, it->second.definition.input_schema, "arguments", validation_error)) {
        return make_response(id, tool_result({{"error", validation_error}}, true, modern));
    }

    try {
        auto result =
            tool_executor_ && it->second.document_domain != McpDocumentDomain::None
                ? tool_executor_(
                      it->second.document_domain, tool_name, arguments, it->second.handler)
                : it->second.handler(arguments);
        const bool is_error = (result.is_object() && result.contains("error")) ||
                              (result.is_object() && result.contains("success") &&
                               result["success"].is_boolean() && !result["success"].get<bool>());
        return make_response(id, tool_result(std::move(result), is_error, modern));
    } catch (const std::exception& e) {
        return make_response(
            id, tool_result({{"error", std::string("Error: ") + e.what()}}, true, modern));
    } catch (...) {
        return make_response(id,
                             tool_result({{"error", "Error: unknown exception"}}, true, modern));
    }
}

nlohmann::json McpServer::make_response(const nlohmann::json& id, const nlohmann::json& result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

nlohmann::json
McpServer::make_error(const nlohmann::json& id, int code, const std::string& message) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
}

} // namespace sunny::infrastructure
