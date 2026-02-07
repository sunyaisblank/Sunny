/**
 * @file MCPS001A.cpp
 * @brief MCP Protocol Handler implementation
 *
 * Component: MCPS001A
 */

#include "MCPS001A.h"

#include <iostream>
#include <string>

namespace Sunny::Infrastructure {

void McpServer::register_tool(std::string name, std::string description,
                               nlohmann::json input_schema,
                               McpToolHandler handler) {
    ToolEntry entry;
    entry.definition.name = name;
    entry.definition.description = std::move(description);
    entry.definition.input_schema = std::move(input_schema);
    entry.handler = std::move(handler);
    tools_[std::move(name)] = std::move(entry);
}

void McpServer::run() {
    running_ = true;
    std::string line;

    while (running_ && std::getline(std::cin, line)) {
        if (line.empty()) continue;

        nlohmann::json request;
        try {
            request = nlohmann::json::parse(line);
        } catch (const nlohmann::json::parse_error&) {
            auto error = make_error(nullptr, -32700, "Parse error");
            std::cout << error.dump() << "\n" << std::flush;
            continue;
        }

        auto response = handle_request(request);
        if (!response.is_null()) {
            std::cout << response.dump() << "\n" << std::flush;
        }
    }

    running_ = false;
}

void McpServer::stop() {
    running_ = false;
}

nlohmann::json McpServer::handle_request(const nlohmann::json& request) {
    // Validate JSON-RPC 2.0
    if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0") {
        auto id = request.value("id", nlohmann::json(nullptr));
        return make_error(id, -32600, "Invalid Request: missing jsonrpc 2.0");
    }

    if (!request.contains("method") || !request["method"].is_string()) {
        auto id = request.value("id", nlohmann::json(nullptr));
        return make_error(id, -32600, "Invalid Request: missing method");
    }

    auto method = request["method"].get<std::string>();
    auto id = request.value("id", nlohmann::json(nullptr));
    auto params = request.value("params", nlohmann::json::object());

    if (method == "initialize") {
        return handle_initialize(id);
    } else if (method == "tools/list") {
        return handle_tools_list(id);
    } else if (method == "tools/call") {
        return handle_tools_call(id, params);
    } else if (method == "notifications/initialized") {
        // Client notification, no response
        return nullptr;
    } else {
        return make_error(id, -32601, "Method not found: " + method);
    }
}

nlohmann::json McpServer::handle_initialize(const nlohmann::json& id) {
    nlohmann::json result = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", {
            {"tools", {
                {"listChanged", false}
            }}
        }},
        {"serverInfo", {
            {"name", "sunny-mcp"},
            {"version", "0.1.0"}
        }}
    };
    return make_response(id, result);
}

nlohmann::json McpServer::handle_tools_list(const nlohmann::json& id) {
    nlohmann::json tools_array = nlohmann::json::array();

    for (const auto& [name, entry] : tools_) {
        tools_array.push_back({
            {"name", entry.definition.name},
            {"description", entry.definition.description},
            {"inputSchema", entry.definition.input_schema}
        });
    }

    return make_response(id, {{"tools", tools_array}});
}

nlohmann::json McpServer::handle_tools_call(const nlohmann::json& id,
                                             const nlohmann::json& params) {
    if (!params.contains("name") || !params["name"].is_string()) {
        return make_error(id, -32602, "Missing tool name");
    }

    auto tool_name = params["name"].get<std::string>();
    auto it = tools_.find(tool_name);
    if (it == tools_.end()) {
        return make_error(id, -32602, "Unknown tool: " + tool_name);
    }

    auto arguments = params.value("arguments", nlohmann::json::object());

    try {
        auto result = it->second.handler(arguments);
        nlohmann::json content = {
            {"content", nlohmann::json::array({
                {{"type", "text"}, {"text", result.dump()}}
            })}
        };
        return make_response(id, content);
    } catch (const std::exception& e) {
        nlohmann::json content = {
            {"content", nlohmann::json::array({
                {{"type", "text"}, {"text", std::string("Error: ") + e.what()}}
            })},
            {"isError", true}
        };
        return make_response(id, content);
    }
}

nlohmann::json McpServer::make_response(const nlohmann::json& id,
                                         const nlohmann::json& result) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    };
}

nlohmann::json McpServer::make_error(const nlohmann::json& id,
                                      int code,
                                      const std::string& message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message}
        }}
    };
}

}  // namespace Sunny::Infrastructure
