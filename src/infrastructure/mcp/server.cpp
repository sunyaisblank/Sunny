/**
 * @file server.cpp
 * @brief MCP Protocol Handler implementation
 *
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sunny/infrastructure/mcp/server.hpp>

namespace sunny::infrastructure {

namespace {

constexpr std::string_view MODERN_PROTOCOL_VERSION = "2026-07-28";
constexpr std::string_view LATEST_LEGACY_PROTOCOL_VERSION = "2025-11-25";
constexpr std::string_view SERVER_NAME = "sunny-mcp";
constexpr std::string_view SERVER_VERSION = "0.4.0";

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
    tools_[std::move(name)] = std::move(entry);
}

void McpServer::run() {
    run(std::cin, std::cout);
}

void McpServer::run(std::istream& input, std::ostream& output) {
    running_.store(true, std::memory_order_relaxed);
    std::string line;

    constexpr std::size_t MAX_LINE_LENGTH = std::size_t{4} * 1024 * 1024; // 4 MiB

    // Replacement keeps one malformed UTF-8 byte in a tool payload from
    // aborting the write and, with it, the session.
    const auto write_line = [&output](const nlohmann::json& message) {
        output << message.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace) << "\n"
               << std::flush;
    };

    // A null result means the message warrants no reply.
    const auto respond = [this](const nlohmann::json& message) -> nlohmann::json {
        try {
            return process_request(message);
        } catch (const std::exception&) {
            // process_request validates every field it reads, so reaching this
            // handler is a server defect. The session survives it; a
            // notification still receives no reply (JSON-RPC 2.0 section 4.1).
            if (!message.is_object() || !message.contains("id")) return nullptr;
            return make_error(request_id_or_null(message), -32603, "Internal error");
        }
    };

    while (running_.load(std::memory_order_relaxed) && std::getline(input, line)) {
        if (line.empty()) continue;

        if (line.size() > MAX_LINE_LENGTH) {
            write_line(make_error(nullptr, -32600, "Request exceeds size limit"));
            continue;
        }

        nlohmann::json message;
        try {
            message = nlohmann::json::parse(line);
        } catch (const nlohmann::json::parse_error&) {
            write_line(make_error(nullptr, -32700, "Parse error"));
            continue;
        }

        // JSON-RPC 2.0 section 6: a batch is answered element by element in one
        // array that omits notifications; an empty batch is itself invalid, and
        // a batch of only notifications produces no output.
        if (message.is_array()) {
            if (message.empty()) {
                write_line(make_error(nullptr, -32600, "Invalid Request: empty batch"));
                continue;
            }
            auto responses = nlohmann::json::array();
            for (const auto& element : message) {
                auto response = respond(element);
                if (!response.is_null()) responses.push_back(std::move(response));
            }
            if (!responses.empty()) write_line(responses);
            continue;
        }
        const auto response = respond(message);
        if (!response.is_null()) write_line(response);
    }

    running_.store(false, std::memory_order_relaxed);
}

void McpServer::stop() {
    running_.store(false, std::memory_order_relaxed);
}

nlohmann::json McpServer::process_request(const nlohmann::json& message) {
    std::unique_lock request_lock(request_mutex_);

    // JSON-RPC 2.0 section 5: when the id cannot be read from an invalid
    // Request, the error response carries a null id. A non-object message
    // (array, string, number, null) has no id to read.
    if (!message.is_object()) {
        return make_error(nullptr, -32600, "Invalid Request: message must be a JSON object");
    }

    // A response to a server-initiated request is never answered (section 5).
    if (!message.contains("method") && (message.contains("result") || message.contains("error"))) {
        return nullptr;
    }

    const auto error_id = request_id_or_null(message);
    if (!message.contains("jsonrpc") || message["jsonrpc"] != "2.0") {
        return make_error(error_id, -32600, "Invalid Request: missing jsonrpc 2.0");
    }
    if (!message.contains("method") || !message["method"].is_string()) {
        return make_error(error_id, -32600, "Invalid Request: missing method");
    }

    // A well-formed Request without an id is a notification (section 4.1):
    // the server must not reply, even when its method or params are invalid.
    // No notification Sunny receives changes server state.
    if (!message.contains("id")) return nullptr;

    // MCP narrows JSON-RPC ids to strings and integers; null is excluded.
    if (error_id.is_null()) {
        return make_error(nullptr, -32600, "Invalid Request: id must be a string or integer");
    }
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
    if (method == "tools/call") return handle_tools_call(id, params, modern);
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
        auto result = it->second.handler(arguments);
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
