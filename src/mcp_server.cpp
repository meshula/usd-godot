#include "mcp_server.h"
#include "mcp_json.h"
#include "version.h"

#include <pxr/pxr.h>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <sstream>
#include <algorithm>

using namespace godot;

namespace mcp {

McpServer::McpServer()
    : running_(false)
    , initialized_(false)
    , plugin_registered_(false) {
}

McpServer::~McpServer() {
    stop();
}

bool McpServer::start() {
    if (running_) {
        return false;
    }

    running_ = true;
    server_thread_ = std::thread(&McpServer::run, this);

    UtilityFunctions::print("MCP Server: Started on stdio");
    return true;
}

void McpServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    UtilityFunctions::print("MCP Server: Stopped");
}

void McpServer::run() {
    std::string line;

    while (running_) {
        // Read from stdin
        if (!std::getline(std::cin, line)) {
            // EOF or error
            break;
        }

        if (line.empty()) {
            continue;
        }

        // Process the request
        process_request(line);
    }
}

void McpServer::process_request(const std::string& request) {
    std::lock_guard<std::mutex> lock(io_mutex_);

    // Extract method name
    std::string method = extract_method(request);
    std::string id = extract_id(request);

    if (method.empty()) {
        // Invalid request - send error
        JsonValue error = JsonValue::object();
        error.set("jsonrpc", JsonValue::string("2.0"));
        if (!id.empty()) {
            error.set("id", JsonValue::string(id));
        } else {
            error.set("id", JsonValue::null());
        }
        JsonValue errorObj = JsonValue::object();
        errorObj.set("code", JsonValue::number(-32700));
        errorObj.set("message", JsonValue::string("Parse error"));
        error.set("error", errorObj);
        send_response(error.to_string());
        return;
    }

    // Handle different methods
    if (method == "initialize") {
        std::string response = handle_initialize(id);
        send_response(response);
        initialized_ = true;
    } else if (method == "initialized") {
        // Client notification that initialization is complete
        // No response needed for notifications
        UtilityFunctions::print("MCP Server: Client initialization complete");
    } else {
        // Method not found
        JsonValue error = JsonValue::object();
        error.set("jsonrpc", JsonValue::string("2.0"));
        if (!id.empty()) {
            error.set("id", JsonValue::string(id));
        } else {
            error.set("id", JsonValue::null());
        }
        JsonValue errorObj = JsonValue::object();
        errorObj.set("code", JsonValue::number(-32601));
        errorObj.set("message", JsonValue::string("Method not found: " + method));
        error.set("error", errorObj);
        send_response(error.to_string());
    }
}

std::string McpServer::handle_initialize(const std::string& id) {
    // Build version information
    std::string plugin_version = GODOT_USD_VERSION_STRING;

    // Get Godot version
    Engine* engine = Engine::get_singleton();
    Dictionary version_info = engine->get_version_info();
    std::ostringstream godot_version;
    godot_version << version_info["major"].operator int64_t() << "."
                  << version_info["minor"].operator int64_t() << "."
                  << version_info["patch"].operator int64_t();

    // Get USD version (PXR_VERSION is in format YYMM, e.g., 2505 = 25.05)
    int pxr_ver = PXR_VERSION;
    int usd_year = pxr_ver / 100;
    int usd_month = pxr_ver % 100;
    std::ostringstream usd_version;
    usd_version << usd_year << "." << std::setw(2) << std::setfill('0') << usd_month;

    // Build MCP initialize response
    JsonValue result = JsonValue::object();

    // Protocol version
    result.set("protocolVersion", JsonValue::string("2024-11-05"));

    // Capabilities
    JsonValue capabilities = JsonValue::object();
    // We'll add tool capabilities later
    result.set("capabilities", capabilities);

    // Server info
    JsonValue serverInfo = JsonValue::object();
    serverInfo.set("name", JsonValue::string("godot-usd"));
    serverInfo.set("version", JsonValue::string(plugin_version));
    result.set("serverInfo", serverInfo);

    // Additional version information
    JsonValue versionInfo = JsonValue::object();
    versionInfo.set("pluginVersion", JsonValue::string(plugin_version));
    versionInfo.set("godotVersion", JsonValue::string(godot_version.str()));
    versionInfo.set("usdVersion", JsonValue::string(usd_version.str()));
    versionInfo.set("pluginRegistered", JsonValue::boolean(plugin_registered_));
    result.set("_meta", versionInfo);

    // Build full response
    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    if (!id.empty()) {
        response.set("id", JsonValue::string(id));
    } else {
        response.set("id", JsonValue::null());
    }
    response.set("result", result);

    UtilityFunctions::print(String("MCP Server: Initialize - Plugin: ") + String(plugin_version.c_str()) +
                           String(", Godot: ") + String(godot_version.str().c_str()) +
                           String(", USD: ") + String(usd_version.str().c_str()) +
                           String(", Registered: ") + String(plugin_registered_ ? "true" : "false"));

    return response.to_string();
}

void McpServer::send_response(const std::string& response) {
    // Send to stdout with proper JSON-RPC formatting
    std::cout << response << std::endl;
    std::cout.flush();
}

std::string McpServer::extract_method(const std::string& request) {
    // Simple JSON parsing to extract "method" field
    // Look for "method":"value" or "method": "value"
    size_t method_pos = request.find("\"method\"");
    if (method_pos == std::string::npos) {
        return "";
    }

    // Find the colon after "method"
    size_t colon_pos = request.find(':', method_pos);
    if (colon_pos == std::string::npos) {
        return "";
    }

    // Find the opening quote of the value
    size_t quote_start = request.find('"', colon_pos);
    if (quote_start == std::string::npos) {
        return "";
    }

    // Find the closing quote
    size_t quote_end = request.find('"', quote_start + 1);
    if (quote_end == std::string::npos) {
        return "";
    }

    return request.substr(quote_start + 1, quote_end - quote_start - 1);
}

std::string McpServer::extract_id(const std::string& request) {
    // Simple JSON parsing to extract "id" field
    size_t id_pos = request.find("\"id\"");
    if (id_pos == std::string::npos) {
        return "";
    }

    // Find the colon after "id"
    size_t colon_pos = request.find(':', id_pos);
    if (colon_pos == std::string::npos) {
        return "";
    }

    // Skip whitespace
    size_t value_start = colon_pos + 1;
    while (value_start < request.length() && std::isspace(request[value_start])) {
        value_start++;
    }

    if (value_start >= request.length()) {
        return "";
    }

    // Check if it's a string (starts with quote) or a number
    if (request[value_start] == '"') {
        // String value
        size_t quote_end = request.find('"', value_start + 1);
        if (quote_end == std::string::npos) {
            return "";
        }
        return request.substr(value_start + 1, quote_end - value_start - 1);
    } else {
        // Number or null value
        size_t value_end = value_start;
        while (value_end < request.length() &&
               (std::isdigit(request[value_end]) || request[value_end] == '.' ||
                request[value_end] == '-' || request[value_end] == 'e' || request[value_end] == 'E')) {
            value_end++;
        }
        return request.substr(value_start, value_end - value_start);
    }
}

} // namespace mcp
