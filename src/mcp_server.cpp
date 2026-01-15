#include "mcp_server.h"
#include "mcp_json.h"
#include "version.h"
#include "usd_stage_manager.h"

#include <pxr/pxr.h>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <sstream>
#include <algorithm>

using namespace godot;
using namespace usd_godot;

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
    } else if (method == "usd/create_stage") {
        std::string response = handle_create_stage(id, request);
        send_response(response);
    } else if (method == "usd/save_stage") {
        std::string response = handle_save_stage(id, request);
        send_response(response);
    } else if (method == "usd/query_generation") {
        std::string response = handle_query_generation(id, request);
        send_response(response);
    } else if (method == "usd/create_prim") {
        std::string response = handle_create_prim(id, request);
        send_response(response);
    } else if (method == "usd/set_attribute") {
        std::string response = handle_set_attribute(id, request);
        send_response(response);
    } else if (method == "usd/get_attribute") {
        std::string response = handle_get_attribute(id, request);
        send_response(response);
    } else if (method == "usd/set_transform") {
        std::string response = handle_set_transform(id, request);
        send_response(response);
    } else if (method == "usd/list_prims") {
        std::string response = handle_list_prims(id, request);
        send_response(response);
    } else {
        // Method not found
        send_error(id, -32601, "Method not found: " + method);
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

    // Capabilities - declare that we support tools
    JsonValue capabilities = JsonValue::object();

    // Build tools array listing all USD commands
    JsonValue tools_array = JsonValue::array();

    // usd/create_stage
    JsonValue tool1 = JsonValue::object();
    tool1.set("name", JsonValue::string("usd/create_stage"));
    tool1.set("description", JsonValue::string("Create a new USD stage (in-memory or file-based)"));
    tools_array.push(tool1);

    // usd/save_stage
    JsonValue tool2 = JsonValue::object();
    tool2.set("name", JsonValue::string("usd/save_stage"));
    tool2.set("description", JsonValue::string("Save a USD stage to file"));
    tools_array.push(tool2);

    // usd/query_generation
    JsonValue tool3 = JsonValue::object();
    tool3.set("name", JsonValue::string("usd/query_generation"));
    tool3.set("description", JsonValue::string("Query stage generation number (tracks modifications)"));
    tools_array.push(tool3);

    // usd/create_prim
    JsonValue tool4 = JsonValue::object();
    tool4.set("name", JsonValue::string("usd/create_prim"));
    tool4.set("description", JsonValue::string("Create a prim with specified type"));
    tools_array.push(tool4);

    // usd/set_attribute
    JsonValue tool5 = JsonValue::object();
    tool5.set("name", JsonValue::string("usd/set_attribute"));
    tool5.set("description", JsonValue::string("Set an attribute on a prim"));
    tools_array.push(tool5);

    // usd/get_attribute
    JsonValue tool6 = JsonValue::object();
    tool6.set("name", JsonValue::string("usd/get_attribute"));
    tool6.set("description", JsonValue::string("Get an attribute value from a prim"));
    tools_array.push(tool6);

    // usd/set_transform
    JsonValue tool7 = JsonValue::object();
    tool7.set("name", JsonValue::string("usd/set_transform"));
    tool7.set("description", JsonValue::string("Set transform (translation, rotation, scale) on a prim"));
    tools_array.push(tool7);

    // usd/list_prims
    JsonValue tool8 = JsonValue::object();
    tool8.set("name", JsonValue::string("usd/list_prims"));
    tool8.set("description", JsonValue::string("List all prims in a stage"));
    tools_array.push(tool8);

    JsonValue tools_capability = JsonValue::object();
    tools_capability.set("tools", tools_array);
    capabilities.set("tools", tools_capability);

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

void McpServer::send_error(const std::string& id, int code, const std::string& message) {
    JsonValue error = JsonValue::object();
    error.set("jsonrpc", JsonValue::string("2.0"));
    if (!id.empty()) {
        error.set("id", JsonValue::string(id));
    } else {
        error.set("id", JsonValue::null());
    }
    JsonValue errorObj = JsonValue::object();
    errorObj.set("code", JsonValue::number(code));
    errorObj.set("message", JsonValue::string(message));
    error.set("error", errorObj);
    send_response(error.to_string());
}

std::string McpServer::extract_string_param(const std::string& request, const std::string& param_name) {
    // Look for "param_name":"value" pattern
    std::string search = "\"" + param_name + "\"";
    size_t param_pos = request.find(search);
    if (param_pos == std::string::npos) {
        return "";
    }

    size_t colon_pos = request.find(':', param_pos);
    if (colon_pos == std::string::npos) {
        return "";
    }

    size_t quote_start = request.find('"', colon_pos);
    if (quote_start == std::string::npos) {
        return "";
    }

    size_t quote_end = request.find('"', quote_start + 1);
    if (quote_end == std::string::npos) {
        return "";
    }

    return request.substr(quote_start + 1, quote_end - quote_start - 1);
}

int64_t McpServer::extract_int_param(const std::string& request, const std::string& param_name) {
    // Look for "param_name":123 pattern
    std::string search = "\"" + param_name + "\"";
    size_t param_pos = request.find(search);
    if (param_pos == std::string::npos) {
        return 0;
    }

    size_t colon_pos = request.find(':', param_pos);
    if (colon_pos == std::string::npos) {
        return 0;
    }

    // Skip whitespace
    size_t value_start = colon_pos + 1;
    while (value_start < request.length() && std::isspace(request[value_start])) {
        value_start++;
    }

    if (value_start >= request.length()) {
        return 0;
    }

    // Parse number
    size_t value_end = value_start;
    while (value_end < request.length() &&
           (std::isdigit(request[value_end]) || request[value_end] == '-')) {
        value_end++;
    }

    std::string value_str = request.substr(value_start, value_end - value_start);
    try {
        return std::stoll(value_str);
    } catch (...) {
        return 0;
    }
}

double McpServer::extract_double_param(const std::string& request, const std::string& param_name) {
    // Look for "param_name":123.45 pattern
    std::string search = "\"" + param_name + "\"";
    size_t param_pos = request.find(search);
    if (param_pos == std::string::npos) {
        return 0.0;
    }

    size_t colon_pos = request.find(':', param_pos);
    if (colon_pos == std::string::npos) {
        return 0.0;
    }

    // Skip whitespace
    size_t value_start = colon_pos + 1;
    while (value_start < request.length() && std::isspace(request[value_start])) {
        value_start++;
    }

    if (value_start >= request.length()) {
        return 0.0;
    }

    // Parse number (including decimal point and scientific notation)
    size_t value_end = value_start;
    while (value_end < request.length() &&
           (std::isdigit(request[value_end]) || request[value_end] == '.' ||
            request[value_end] == '-' || request[value_end] == '+' ||
            request[value_end] == 'e' || request[value_end] == 'E')) {
        value_end++;
    }

    std::string value_str = request.substr(value_start, value_end - value_start);
    try {
        return std::stod(value_str);
    } catch (...) {
        return 0.0;
    }
}

std::string McpServer::handle_create_stage(const std::string& id, const std::string& request) {
    // Extract parameters
    std::string file_path = extract_string_param(request, "file_path");

    // Create stage using stage manager
    StageId stage_id = UsdStageManager::get_singleton().create_stage(file_path);

    if (stage_id == 0) {
        send_error(id, -32000, "Failed to create USD stage");
        return "";
    }

    // Build response
    JsonValue result = JsonValue::object();
    result.set("stage_id", JsonValue::number(static_cast<double>(stage_id)));
    result.set("generation", JsonValue::number(0));

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    UtilityFunctions::print(String("MCP Server: Created stage ") + String::num_int64(stage_id));

    return response.to_string();
}

std::string McpServer::handle_save_stage(const std::string& id, const std::string& request) {
    // Extract parameters
    int64_t stage_id = extract_int_param(request, "stage_id");
    std::string file_path = extract_string_param(request, "file_path");

    if (stage_id == 0) {
        send_error(id, -32602, "Invalid stage_id parameter");
        return "";
    }

    // Save stage using stage manager
    bool success = UsdStageManager::get_singleton().save_stage(stage_id, file_path);

    if (!success) {
        send_error(id, -32000, "Failed to save USD stage");
        return "";
    }

    // Build response
    JsonValue result = JsonValue::object();
    result.set("success", JsonValue::boolean(true));
    result.set("stage_id", JsonValue::number(static_cast<double>(stage_id)));

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    UtilityFunctions::print(String("MCP Server: Saved stage ") + String::num_int64(stage_id));

    return response.to_string();
}

std::string McpServer::handle_query_generation(const std::string& id, const std::string& request) {
    // Extract parameters
    int64_t stage_id = extract_int_param(request, "stage_id");

    if (stage_id == 0) {
        send_error(id, -32602, "Invalid stage_id parameter");
        return "";
    }

    // Get generation from stage manager
    uint64_t generation = UsdStageManager::get_singleton().get_generation(stage_id);

    // Build response
    JsonValue result = JsonValue::object();
    result.set("stage_id", JsonValue::number(static_cast<double>(stage_id)));
    result.set("generation", JsonValue::number(static_cast<double>(generation)));

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    return response.to_string();
}

std::string McpServer::handle_create_prim(const std::string& id, const std::string& request) {
    // Extract parameters
    int64_t stage_id = extract_int_param(request, "stage_id");
    std::string prim_path = extract_string_param(request, "prim_path");
    std::string prim_type = extract_string_param(request, "prim_type");

    if (stage_id == 0) {
        send_error(id, -32602, "Invalid stage_id parameter");
        return "";
    }

    if (prim_path.empty()) {
        send_error(id, -32602, "Missing prim_path parameter");
        return "";
    }

    // Create prim using stage manager
    bool success = UsdStageManager::get_singleton().create_prim(stage_id, prim_path, prim_type);

    if (!success) {
        send_error(id, -32000, "Failed to create prim");
        return "";
    }

    // Get updated generation
    uint64_t generation = UsdStageManager::get_singleton().get_generation(stage_id);

    // Build response
    JsonValue result = JsonValue::object();
    result.set("success", JsonValue::boolean(true));
    result.set("stage_id", JsonValue::number(static_cast<double>(stage_id)));
    result.set("prim_path", JsonValue::string(prim_path));
    result.set("generation", JsonValue::number(static_cast<double>(generation)));

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    UtilityFunctions::print(String("MCP Server: Created prim ") + String(prim_path.c_str()) +
                           String(" in stage ") + String::num_int64(stage_id));

    return response.to_string();
}

std::string McpServer::handle_set_attribute(const std::string& id, const std::string& request) {
    // Extract parameters
    int64_t stage_id = extract_int_param(request, "stage_id");
    std::string prim_path = extract_string_param(request, "prim_path");
    std::string attr_name = extract_string_param(request, "attr_name");
    std::string value_type = extract_string_param(request, "value_type");
    std::string value = extract_string_param(request, "value");

    if (stage_id == 0) {
        send_error(id, -32602, "Invalid stage_id parameter");
        return "";
    }

    if (prim_path.empty()) {
        send_error(id, -32602, "Missing prim_path parameter");
        return "";
    }

    if (attr_name.empty()) {
        send_error(id, -32602, "Missing attr_name parameter");
        return "";
    }

    if (value_type.empty()) {
        send_error(id, -32602, "Missing value_type parameter");
        return "";
    }

    // Set attribute using stage manager
    bool success = UsdStageManager::get_singleton().set_prim_attribute(
        stage_id, prim_path, attr_name, value_type, value);

    if (!success) {
        send_error(id, -32000, "Failed to set attribute");
        return "";
    }

    // Get updated generation
    uint64_t generation = UsdStageManager::get_singleton().get_generation(stage_id);

    // Build response
    JsonValue result = JsonValue::object();
    result.set("success", JsonValue::boolean(true));
    result.set("stage_id", JsonValue::number(static_cast<double>(stage_id)));
    result.set("prim_path", JsonValue::string(prim_path));
    result.set("attr_name", JsonValue::string(attr_name));
    result.set("generation", JsonValue::number(static_cast<double>(generation)));

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    UtilityFunctions::print(String("MCP Server: Set attribute ") + String(attr_name.c_str()) +
                           String(" on prim ") + String(prim_path.c_str()) +
                           String(" in stage ") + String::num_int64(stage_id));

    return response.to_string();
}

std::string McpServer::handle_get_attribute(const std::string& id, const std::string& request) {
    // Extract parameters
    int64_t stage_id = extract_int_param(request, "stage_id");
    std::string prim_path = extract_string_param(request, "prim_path");
    std::string attr_name = extract_string_param(request, "attr_name");

    if (stage_id == 0) {
        send_error(id, -32602, "Invalid stage_id parameter");
        return "";
    }

    if (prim_path.empty()) {
        send_error(id, -32602, "Missing prim_path parameter");
        return "";
    }

    if (attr_name.empty()) {
        send_error(id, -32602, "Missing attr_name parameter");
        return "";
    }

    // Get attribute using stage manager
    std::string value;
    std::string value_type;
    bool success = UsdStageManager::get_singleton().get_prim_attribute(
        stage_id, prim_path, attr_name, value, value_type);

    if (!success) {
        send_error(id, -32000, "Failed to get attribute");
        return "";
    }

    // Build response
    JsonValue result = JsonValue::object();
    result.set("stage_id", JsonValue::number(static_cast<double>(stage_id)));
    result.set("prim_path", JsonValue::string(prim_path));
    result.set("attr_name", JsonValue::string(attr_name));
    result.set("value", JsonValue::string(value));
    result.set("value_type", JsonValue::string(value_type));

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    return response.to_string();
}

std::string McpServer::handle_set_transform(const std::string& id, const std::string& request) {
    // Extract parameters
    int64_t stage_id = extract_int_param(request, "stage_id");
    std::string prim_path = extract_string_param(request, "prim_path");

    // Extract transform components
    double tx = extract_double_param(request, "tx");
    double ty = extract_double_param(request, "ty");
    double tz = extract_double_param(request, "tz");
    double rx = extract_double_param(request, "rx");
    double ry = extract_double_param(request, "ry");
    double rz = extract_double_param(request, "rz");
    double sx = extract_double_param(request, "sx");
    double sy = extract_double_param(request, "sy");
    double sz = extract_double_param(request, "sz");

    if (stage_id == 0) {
        send_error(id, -32602, "Invalid stage_id parameter");
        return "";
    }

    if (prim_path.empty()) {
        send_error(id, -32602, "Missing prim_path parameter");
        return "";
    }

    // Set transform using stage manager
    bool success = UsdStageManager::get_singleton().set_prim_transform(
        stage_id, prim_path, tx, ty, tz, rx, ry, rz, sx, sy, sz);

    if (!success) {
        send_error(id, -32000, "Failed to set transform");
        return "";
    }

    // Get updated generation
    uint64_t generation = UsdStageManager::get_singleton().get_generation(stage_id);

    // Build response
    JsonValue result = JsonValue::object();
    result.set("success", JsonValue::boolean(true));
    result.set("stage_id", JsonValue::number(static_cast<double>(stage_id)));
    result.set("prim_path", JsonValue::string(prim_path));
    result.set("generation", JsonValue::number(static_cast<double>(generation)));

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    UtilityFunctions::print(String("MCP Server: Set transform on prim ") + String(prim_path.c_str()) +
                           String(" in stage ") + String::num_int64(stage_id));

    return response.to_string();
}

std::string McpServer::handle_list_prims(const std::string& id, const std::string& request) {
    // Extract parameters
    int64_t stage_id = extract_int_param(request, "stage_id");

    if (stage_id == 0) {
        send_error(id, -32602, "Invalid stage_id parameter");
        return "";
    }

    // Get prim list from stage manager
    std::vector<std::string> prim_paths = UsdStageManager::get_singleton().list_prims(stage_id);

    // Build response with array of prim paths
    JsonValue prims_array = JsonValue::array();
    for (const std::string& path : prim_paths) {
        prims_array.push(JsonValue::string(path));
    }

    JsonValue result = JsonValue::object();
    result.set("stage_id", JsonValue::number(static_cast<double>(stage_id)));
    result.set("prims", prims_array);
    result.set("count", JsonValue::number(static_cast<double>(prim_paths.size())));

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    UtilityFunctions::print(String("MCP Server: Listed ") + String::num_int64(prim_paths.size()) +
                           String(" prims in stage ") + String::num_int64(stage_id));

    return response.to_string();
}

} // namespace mcp
