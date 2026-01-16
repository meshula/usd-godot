#include "mcp_server.h"
#include "mcp_json.h"
#include "mcp_globals.h"
#include "version.h"
#include "usd_stage_manager.h"
#include "usd_stage_group_mapping.h"

#include <pxr/pxr.h>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <sstream>
#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#include <unistd.h>
#endif

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
#ifndef _WIN32
        // Use select() to check if stdin has data available with a timeout
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms timeout

        int result = select(STDIN_FILENO + 1, &read_fds, nullptr, nullptr, &timeout);

        if (result < 0) {
            // Error in select
            break;
        } else if (result == 0) {
            // Timeout - no data available, continue loop to check running_ flag
            continue;
        }
#else
        // Windows - just sleep briefly and check running_ flag
        // (Windows doesn't support select() on stdin)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Try non-blocking check if data is available
        // Note: This is simplified for Windows, may need improvement
#endif

        // Data is available, read it
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

    UtilityFunctions::print("MCP Server: Thread exiting cleanly");
}

void McpServer::process_request(const std::string& request) {
    std::string response = process_request_sync(request);
    if (!response.empty()) {
        std::lock_guard<std::mutex> lock(io_mutex_);
        send_response(response);
    }
}

std::string McpServer::process_request_sync(const std::string& request) {
    // Extract method name
    std::string method = extract_method(request);
    std::string id = extract_id(request);

    if (method.empty()) {
        // Invalid request - return error
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
        return error.to_string();
    }

    // Handle different methods
    if (method == "initialize") {
        initialized_ = true;
        return handle_initialize(id);
    } else if (method == "initialized") {
        // Client notification that initialization is complete
        // No response needed for notifications
        UtilityFunctions::print("MCP Server: Client initialization complete");
        return ""; // No response for notifications
    } else if (method == "usd/create_stage") {
        return handle_create_stage(id, request);
    } else if (method == "usd/save_stage") {
        return handle_save_stage(id, request);
    } else if (method == "usd/query_generation") {
        return handle_query_generation(id, request);
    } else if (method == "usd/create_prim") {
        return handle_create_prim(id, request);
    } else if (method == "usd/set_attribute") {
        return handle_set_attribute(id, request);
    } else if (method == "usd/get_attribute") {
        return handle_get_attribute(id, request);
    } else if (method == "usd/set_transform") {
        return handle_set_transform(id, request);
    } else if (method == "usd/list_prims") {
        return handle_list_prims(id, request);
    } else if (method == "usd/list_stages") {
        return handle_list_stages(id, request);
    } else if (method == "usd/create_scene_group") {
        return handle_create_scene_group(id, request);
    } else if (method == "usd/reflect_to_scene") {
        return handle_reflect_to_scene(id, request);
    } else if (method == "usd/confirm_reflect") {
        return handle_confirm_reflect(id, request);
    } else if (method == "godot/query_scene_tree") {
        return handle_query_scene_tree(id, request);
    } else if (method == "godot/dtack") {
        return handle_dtack(id, request);
    } else if (method == "godot/get_node_properties") {
        return handle_get_node_properties(id, request);
    } else if (method == "godot/update_node_property") {
        return handle_update_node_property(id, request);
    } else if (method == "godot/duplicate_node") {
        return handle_duplicate_node(id, request);
    } else if (method == "godot/save_scene") {
        return handle_save_scene(id, request);
    } else if (method == "godot/get_bounding_box") {
        return handle_get_bounding_box(id, request);
    } else if (method == "godot/get_selection") {
        return handle_get_selection(id, request);
    } else {
        // Method not found
        JsonValue error = JsonValue::object();
        error.set("jsonrpc", JsonValue::string("2.0"));
        error.set("id", JsonValue::string(id));
        JsonValue errorObj = JsonValue::object();
        errorObj.set("code", JsonValue::number(-32601));
        errorObj.set("message", JsonValue::string("Method not found: " + method));
        error.set("error", errorObj);
        return error.to_string();
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

    // usd/list_stages (Phase 4)
    JsonValue tool9 = JsonValue::object();
    tool9.set("name", JsonValue::string("usd/list_stages"));
    tool9.set("description", JsonValue::string("List all open USD stages with their file paths, generations, and group mappings"));
    tools_array.push(tool9);

    // usd/create_scene_group
    JsonValue tool10 = JsonValue::object();
    tool10.set("name", JsonValue::string("usd/create_scene_group"));
    tool10.set("description", JsonValue::string("Associate a USD file with a scene group name for importing"));
    tools_array.push(tool10);

    // usd/reflect_to_scene
    JsonValue tool11 = JsonValue::object();
    tool11.set("name", JsonValue::string("usd/reflect_to_scene"));
    tool11.set("description", JsonValue::string("Import a USD stage to the current scene as a group. Returns confirmation token if group exists."));
    tools_array.push(tool11);

    // usd/confirm_reflect
    JsonValue tool12 = JsonValue::object();
    tool12.set("name", JsonValue::string("usd/confirm_reflect"));
    tool12.set("description", JsonValue::string("Confirm a pending reflect operation using the confirmation token"));
    tools_array.push(tool12);

    // godot/query_scene_tree
    JsonValue tool13 = JsonValue::object();
    tool13.set("name", JsonValue::string("godot/query_scene_tree"));
    tool13.set("description", JsonValue::string("Query the Godot scene tree at a specific path (ACK/DTACK pattern). Returns ACK token immediately. Poll with godot/dtack to get results."));
    tools_array.push(tool13);

    // godot/dtack
    JsonValue tool14 = JsonValue::object();
    tool14.set("name", JsonValue::string("godot/dtack"));
    tool14.set("description", JsonValue::string("Poll async operation status using ACK token. Returns status (pending/complete/error/canceled) and result data when complete. Pass 'cancel':true to cancel operation."));
    tools_array.push(tool14);

    // godot/get_node_properties (Phase 1)
    JsonValue tool15 = JsonValue::object();
    tool15.set("name", JsonValue::string("godot/get_node_properties"));
    tool15.set("description", JsonValue::string("Get all properties of a node in the scene. Returns property names and values as JSON. Params: node_path (relative to scene root)."));
    tools_array.push(tool15);

    // godot/update_node_property (Phase 1)
    JsonValue tool16 = JsonValue::object();
    tool16.set("name", JsonValue::string("godot/update_node_property"));
    tool16.set("description", JsonValue::string("Update a property on a node. Params: node_path, property, value. Returns success confirmation."));
    tools_array.push(tool16);

    // godot/duplicate_node (Phase 1)
    JsonValue tool17 = JsonValue::object();
    tool17.set("name", JsonValue::string("godot/duplicate_node"));
    tool17.set("description", JsonValue::string("Duplicate a node and all its children. Params: node_path, new_name (optional). Returns new node path."));
    tools_array.push(tool17);

    // godot/save_scene (Phase 1)
    JsonValue tool18 = JsonValue::object();
    tool18.set("name", JsonValue::string("godot/save_scene"));
    tool18.set("description", JsonValue::string("Save the current scene. Params: path (optional, uses current scene path if empty). Returns saved scene path."));
    tools_array.push(tool18);

    // godot/get_bounding_box (Phase 1)
    JsonValue tool19 = JsonValue::object();
    tool19.set("name", JsonValue::string("godot/get_bounding_box"));
    tool19.set("description", JsonValue::string("Get the axis-aligned bounding box (AABB) of a node and all its children. Params: node_path. Returns min/max bounds and size in world space."));
    tools_array.push(tool19);

    // godot/get_selection (Phase 1)
    JsonValue tool20 = JsonValue::object();
    tool20.set("name", JsonValue::string("godot/get_selection"));
    tool20.set("description", JsonValue::string("Get the currently selected nodes in the Godot editor. No params required. Returns array of selected node paths and their types."));
    tools_array.push(tool20);

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

std::string McpServer::build_error(const std::string& id, int code, const std::string& message) {
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
    return error.to_string();
}

void McpServer::add_metadata_to_result(JsonValue& result) {
    // Get user notes from global
    std::string user_notes = usd_godot::get_user_notes();

    // Only add _meta if there are notes (keep responses clean)
    if (!user_notes.empty()) {
        JsonValue meta = JsonValue::object();
        meta.set("notes", JsonValue::string(user_notes));
        result.set("_meta", meta);
    }
}

void McpServer::log_operation(const std::string& operation, const std::string& details) {
    if (log_callback_) {
        log_callback_(operation, details);
    }
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

bool McpServer::extract_bool_param(const std::string& request, const std::string& param_name) {
    // Look for "param_name":true or "param_name":false pattern
    std::string search = "\"" + param_name + "\"";
    size_t param_pos = request.find(search);
    if (param_pos == std::string::npos) {
        return false;
    }

    size_t colon_pos = request.find(':', param_pos);
    if (colon_pos == std::string::npos) {
        return false;
    }

    // Skip whitespace
    size_t value_start = colon_pos + 1;
    while (value_start < request.length() && std::isspace(request[value_start])) {
        value_start++;
    }

    if (value_start >= request.length()) {
        return false;
    }

    // Check for "true" or "false"
    if (request.substr(value_start, 4) == "true") {
        return true;
    }
    if (request.substr(value_start, 5) == "false") {
        return false;
    }

    // Also check for 1 or 0
    if (request[value_start] == '1') {
        return true;
    }
    if (request[value_start] == '0') {
        return false;
    }

    return false;
}

std::string McpServer::generate_ack_token() {
    return "ack_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

std::string McpServer::handle_create_stage(const std::string& id, const std::string& request) {
    // Extract parameters
    std::string file_path = extract_string_param(request, "file_path");

    // Create stage using stage manager
    StageId stage_id = UsdStageManager::get_singleton().create_stage(file_path);

    if (stage_id == 0) {
        log_operation("Create Stage Failed", file_path.empty() ? "(in-memory)" : file_path);
        return build_error(id, -32000, "Failed to create USD stage");
    }

    // Log operation
    std::string details = "Stage ID: " + std::to_string(stage_id);
    if (!file_path.empty()) {
        details += ", Path: " + file_path;
    } else {
        details += " (in-memory)";
    }
    log_operation("Create Stage", details);

    // Build response
    JsonValue result = JsonValue::object();
    result.set("stage_id", JsonValue::number(static_cast<double>(stage_id)));
    result.set("generation", JsonValue::number(0));
    add_metadata_to_result(result);

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
        return build_error(id, -32602, "Invalid stage_id parameter");
    }

    // Save stage using stage manager
    bool success = UsdStageManager::get_singleton().save_stage(stage_id, file_path);

    if (!success) {
        log_operation("Save Stage Failed", "Stage ID: " + std::to_string(stage_id));
        return build_error(id, -32000, "Failed to save USD stage");
    }

    // Log operation
    std::string details = "Stage ID: " + std::to_string(stage_id);
    if (!file_path.empty()) {
        details += ", Path: " + file_path;
    }
    log_operation("Save Stage", details);

    // Build response
    JsonValue result = JsonValue::object();
    add_metadata_to_result(result);
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
        return build_error(id, -32602, "Invalid stage_id parameter");
    }

    // Get generation from stage manager
    uint64_t generation = UsdStageManager::get_singleton().get_generation(stage_id);

    // Build response
    JsonValue result = JsonValue::object();
    result.set("stage_id", JsonValue::number(static_cast<double>(stage_id)));
    add_metadata_to_result(result);
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
        return build_error(id, -32602, "Invalid stage_id parameter");
    }

    if (prim_path.empty()) {
        return build_error(id, -32602, "Missing prim_path parameter");
    }

    // Create prim using stage manager
    bool success = UsdStageManager::get_singleton().create_prim(stage_id, prim_path, prim_type);

    if (!success) {
        log_operation("Create Prim Failed", prim_path + " on Stage " + std::to_string(stage_id));
        return build_error(id, -32000, "Failed to create prim");
    }

    // Get updated generation
    uint64_t generation = UsdStageManager::get_singleton().get_generation(stage_id);

    // Log operation
    std::string details = prim_path;
    if (!prim_type.empty()) {
        details += " (" + prim_type + ")";
    }
    details += " on Stage " + std::to_string(stage_id);
    log_operation("Create Prim", details);

    // Build response
    JsonValue result = JsonValue::object();
    result.set("success", JsonValue::boolean(true));
    add_metadata_to_result(result);
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
        return build_error(id, -32602, "Invalid stage_id parameter");
    }

    if (prim_path.empty()) {
        return build_error(id, -32602, "Missing prim_path parameter");
    }

    if (attr_name.empty()) {
        return build_error(id, -32602, "Missing attr_name parameter");
    }

    if (value_type.empty()) {
        return build_error(id, -32602, "Missing value_type parameter");
    }

    // Set attribute using stage manager
    bool success = UsdStageManager::get_singleton().set_prim_attribute(
        stage_id, prim_path, attr_name, value_type, value);

    if (!success) {
        log_operation("Set Attribute Failed", prim_path + "." + attr_name);
        return build_error(id, -32000, "Failed to set attribute");
    }

    // Get updated generation
    uint64_t generation = UsdStageManager::get_singleton().get_generation(stage_id);

    // Log operation
    std::string details = prim_path + "." + attr_name + " = " + value + " (" + value_type + ")";
    log_operation("Set Attribute", details);

    // Build response
    JsonValue result = JsonValue::object();
    result.set("success", JsonValue::boolean(true));
    add_metadata_to_result(result);
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
        return build_error(id, -32602, "Invalid stage_id parameter");
    }

    if (prim_path.empty()) {
        return build_error(id, -32602, "Missing prim_path parameter");
    }

    if (attr_name.empty()) {
        return build_error(id, -32602, "Missing attr_name parameter");
    }

    // Get attribute using stage manager
    std::string value;
    std::string value_type;
    bool success = UsdStageManager::get_singleton().get_prim_attribute(
        stage_id, prim_path, attr_name, value, value_type);

    if (!success) {
        return build_error(id, -32000, "Failed to get attribute");
    }

    // Build response
    JsonValue result = JsonValue::object();
    result.set("stage_id", JsonValue::number(static_cast<double>(stage_id)));
    add_metadata_to_result(result);
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
        return build_error(id, -32602, "Invalid stage_id parameter");
    }

    if (prim_path.empty()) {
        return build_error(id, -32602, "Missing prim_path parameter");
    }

    // Set transform using stage manager
    bool success = UsdStageManager::get_singleton().set_prim_transform(
        stage_id, prim_path, tx, ty, tz, rx, ry, rz, sx, sy, sz);

    if (!success) {
        log_operation("Set Transform Failed", prim_path);
        return build_error(id, -32000, "Failed to set transform");
    }

    // Get updated generation
    uint64_t generation = UsdStageManager::get_singleton().get_generation(stage_id);

    // Log operation
    std::ostringstream details;
    details << prim_path << " - T(" << tx << "," << ty << "," << tz << ") ";
    details << "R(" << rx << "," << ry << "," << rz << ") ";
    details << "S(" << sx << "," << sy << "," << sz << ")";
    log_operation("Set Transform", details.str());

    // Build response
    JsonValue result = JsonValue::object();
    result.set("success", JsonValue::boolean(true));
    add_metadata_to_result(result);
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
        return build_error(id, -32602, "Invalid stage_id parameter");
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
    add_metadata_to_result(result);
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

// Phase 4: USD Stage Manager Panel Commands

std::string McpServer::handle_list_stages(const std::string& id, const std::string& request) {
    log_operation("usd/list_stages", "Listing all USD stages");

    usd_godot::UsdStageManager& manager = usd_godot::UsdStageManager::get_singleton();
    godot::UsdStageGroupMapping* mapping = godot::UsdStageGroupMapping::get_singleton();

    std::vector<usd_godot::StageId> active_stages = manager.get_active_stages();

    JsonValue stages_array = JsonValue::array();

    for (size_t i = 0; i < active_stages.size(); i++) {
        usd_godot::StageId stage_id = active_stages[i];
        usd_godot::StageRecord* record = manager.get_stage_record(stage_id);
        if (!record) continue;

        uint64_t generation = record->get_generation();
        bool is_loaded = record->is_loaded();

        // Get file path from record (works whether stage is loaded or not)
        std::string file_path = record->get_file_path();
        godot::String godot_file_path(file_path.c_str());

        // Check mapping status
        bool has_mapping = mapping && mapping->has_mapping(godot_file_path);
        std::string group_name;
        bool needs_update = false;

        if (has_mapping) {
            godot::String gname = mapping->get_group_name(godot_file_path);
            group_name = gname.utf8().get_data();
            needs_update = mapping->needs_update(godot_file_path, generation);
        }

        std::string status = !is_loaded ? "not_loaded" :
                           (!has_mapping ? "not_reflected" :
                           (needs_update ? "modified" : "up_to_date"));

        JsonValue stage_info = JsonValue::object();
        stage_info.set("stage_id", JsonValue::number(static_cast<double>(stage_id)));
        stage_info.set("file_path", JsonValue::string(file_path));
        stage_info.set("generation", JsonValue::number(static_cast<double>(generation)));
        stage_info.set("group_name", JsonValue::string(has_mapping ? group_name : ""));
        stage_info.set("status", JsonValue::string(status));

        stages_array.push(stage_info);
    }

    JsonValue result = JsonValue::object();
    result.set("stages", stages_array);
    add_metadata_to_result(result);

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    log_operation("usd/list_stages", "Found " + std::to_string(active_stages.size()) + " stages");
    return response.to_string();
}

std::string McpServer::handle_create_scene_group(const std::string& id, const std::string& request) {
    std::string file_path = extract_string_param(request, "file_path");
    std::string group_name = extract_string_param(request, "group_name");

    if (file_path.empty() || group_name.empty()) {
        return build_error(id, -32602, "Missing required parameters: file_path and group_name");
    }

    log_operation("usd/create_scene_group", "Creating mapping and registering stage: " + file_path + " -> " + group_name);

    godot::UsdStageGroupMapping* mapping = godot::UsdStageGroupMapping::get_singleton();
    if (!mapping) {
        return build_error(id, -32603, "UsdStageGroupMapping not available");
    }

    godot::String godot_file_path(file_path.c_str());
    godot::String godot_group_name(group_name.c_str());

    // Register the stage in the manager (but don't load it yet)
    usd_godot::UsdStageManager& manager = usd_godot::UsdStageManager::get_singleton();
    usd_godot::StageId stage_id = manager.register_stage(file_path);

    // Create the mapping
    mapping->set_mapping(godot_file_path, godot_group_name);

    JsonValue result = JsonValue::object();
    result.set("success", JsonValue::boolean(true));
    result.set("stage_id", JsonValue::number(static_cast<double>(stage_id)));
    result.set("file_path", JsonValue::string(file_path));
    result.set("group_name", JsonValue::string(group_name));
    result.set("status", JsonValue::string("ready_to_reflect"));
    add_metadata_to_result(result);

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    log_operation("usd/create_scene_group", "Stage registered (ID " + std::to_string(stage_id) + ") and mapping created successfully");
    return response.to_string();
}

std::string McpServer::handle_reflect_to_scene(const std::string& id, const std::string& request) {
    std::string file_path = extract_string_param(request, "file_path");
    bool force = extract_bool_param(request, "force");

    if (file_path.empty()) {
        return build_error(id, -32602, "Missing required parameter: file_path");
    }

    log_operation("usd/reflect_to_scene", "Reflecting " + file_path + " (force=" + (force ? "true" : "false") + ")");

    godot::UsdStageGroupMapping* mapping = godot::UsdStageGroupMapping::get_singleton();
    if (!mapping) {
        return build_error(id, -32603, "UsdStageGroupMapping not available");
    }

    godot::String godot_file_path(file_path.c_str());

    if (!mapping->has_mapping(godot_file_path)) {
        return build_error(id, -32603, "No group mapping for this file. Create one with usd/create_scene_group first.");
    }

    godot::String group_name = mapping->get_group_name(godot_file_path);
    std::string group_name_str = group_name.utf8().get_data();

    // TODO: Check if group exists with nodes (for now, just return confirmation_required)
    // This will be fully implemented in Phase 5 when we integrate with USDPlugin

    if (!force) {
        // Generate confirmation token
        std::string token = "reflect_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

        {
            std::lock_guard<std::mutex> lock(confirmations_mutex_);
            pending_confirmations_[token] = {file_path, group_name_str};
        }

        JsonValue result = JsonValue::object();
        result.set("status", JsonValue::string("confirmation_required"));
        result.set("message", JsonValue::string("Group '" + group_name_str + "' may already exist. Use usd/confirm_reflect with token to proceed."));
        result.set("file_path", JsonValue::string(file_path));
        result.set("group_name", JsonValue::string(group_name_str));
        result.set("confirmation_token", JsonValue::string(token));
        add_metadata_to_result(result);

        JsonValue response = JsonValue::object();
        response.set("jsonrpc", JsonValue::string("2.0"));
        response.set("id", JsonValue::string(id));
        response.set("result", result);

        log_operation("usd/reflect_to_scene", "Confirmation required for " + group_name_str);
        return response.to_string();
    }

    // Force mode - proceed directly with import
    if (!import_callback_) {
        return build_error(id, -32603, "Import functionality not available");
    }

    // Call import callback (runs on main thread via call_deferred)
    int node_count = import_callback_(file_path, group_name_str, true);

    if (node_count < 0) {
        return build_error(id, -32603, "Failed to import USD to scene");
    }

    // Update generation after successful import
    usd_godot::UsdStageManager& manager = usd_godot::UsdStageManager::get_singleton();
    uint64_t generation = 0;

    // Find stage by file path
    std::vector<usd_godot::StageId> stages = manager.get_active_stages();
    for (usd_godot::StageId stage_id : stages) {
        usd_godot::StageRecord* record = manager.get_stage_record(stage_id);
        if (record && record->get_file_path() == file_path) {
            generation = record->get_generation();
            break;
        }
    }

    mapping->update_generation(godot_file_path, generation);

    JsonValue result = JsonValue::object();
    result.set("success", JsonValue::boolean(true));
    result.set("file_path", JsonValue::string(file_path));
    result.set("group_name", JsonValue::string(group_name_str));
    result.set("nodes_created", JsonValue::number(static_cast<double>(node_count)));
    result.set("generation", JsonValue::number(static_cast<double>(generation)));
    add_metadata_to_result(result);

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    log_operation("usd/reflect_to_scene", "Imported " + std::to_string(node_count) + " nodes to group '" + group_name_str + "'");
    return response.to_string();
}

std::string McpServer::handle_confirm_reflect(const std::string& id, const std::string& request) {
    std::string token = extract_string_param(request, "confirmation_token");

    if (token.empty()) {
        return build_error(id, -32602, "Missing required parameter: confirmation_token");
    }

    log_operation("usd/confirm_reflect", "Confirming with token: " + token);

    ReflectConfirmation confirmation;
    {
        std::lock_guard<std::mutex> lock(confirmations_mutex_);
        auto it = pending_confirmations_.find(token);
        if (it == pending_confirmations_.end()) {
            return build_error(id, -32603, "Invalid or expired confirmation token");
        }
        confirmation = it->second;
        pending_confirmations_.erase(it);
    }

    // Execute actual import
    if (!import_callback_) {
        return build_error(id, -32603, "Import functionality not available");
    }

    // Call import callback with force=true (user already confirmed)
    int node_count = import_callback_(confirmation.file_path, confirmation.group_name, true);

    if (node_count < 0) {
        return build_error(id, -32603, "Failed to import USD to scene");
    }

    // Update generation after successful import
    usd_godot::UsdStageManager& manager = usd_godot::UsdStageManager::get_singleton();
    godot::UsdStageGroupMapping* mapping = godot::UsdStageGroupMapping::get_singleton();
    uint64_t generation = 0;

    // Find stage by file path
    std::vector<usd_godot::StageId> stages = manager.get_active_stages();
    for (usd_godot::StageId stage_id : stages) {
        usd_godot::StageRecord* record = manager.get_stage_record(stage_id);
        if (record && record->get_file_path() == confirmation.file_path) {
            generation = record->get_generation();
            break;
        }
    }

    if (mapping) {
        godot::String godot_file_path(confirmation.file_path.c_str());
        mapping->update_generation(godot_file_path, generation);
    }

    JsonValue result = JsonValue::object();
    result.set("success", JsonValue::boolean(true));
    result.set("file_path", JsonValue::string(confirmation.file_path));
    result.set("group_name", JsonValue::string(confirmation.group_name));
    result.set("nodes_created", JsonValue::number(static_cast<double>(node_count)));
    result.set("generation", JsonValue::number(static_cast<double>(generation)));
    add_metadata_to_result(result);

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    log_operation("usd/confirm_reflect", "Imported " + std::to_string(node_count) + " nodes to group '" + confirmation.group_name + "'");
    return response.to_string();
}

std::string McpServer::handle_query_scene_tree(const std::string& id, const std::string& request) {
    std::string path = extract_string_param(request, "path");

    // Default to "/" for root if no path specified
    if (path.empty()) {
        path = "/";
    }

    log_operation("godot/query_scene_tree", "ACK: Querying scene tree at path: " + path);

    // Generate ACK token
    std::string ack = generate_ack_token();

    // Create async operation
    AsyncOperation op;
    op.ack_token = ack;
    op.status = "pending";
    op.message = "Querying scene tree at " + path;
    op.result_data = "";

    // Store operation
    {
        std::lock_guard<std::mutex> lock(async_operations_mutex_);
        async_operations_[ack] = op;
    }

    // Schedule query on main thread via callback
    if (query_scene_callback_) {
        // Use callback to schedule work on main thread
        // The callback will update the async_operations_ map when done
        std::thread([this, ack, path]() {
            try {
                std::string result = query_scene_callback_(path);

                std::lock_guard<std::mutex> lock(async_operations_mutex_);
                auto it = async_operations_.find(ack);
                if (it != async_operations_.end()) {
                    if (it->second.status != "canceled") {
                        it->second.status = "complete";
                        it->second.result_data = result;
                        it->second.message = "Query complete";
                    }
                }
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(async_operations_mutex_);
                auto it = async_operations_.find(ack);
                if (it != async_operations_.end()) {
                    it->second.status = "error";
                    it->second.message = std::string("Query failed: ") + e.what();
                }
            }
        }).detach();
    }

    // Return ACK immediately
    JsonValue result = JsonValue::object();
    result.set("ack", JsonValue::string(ack));
    result.set("message", JsonValue::string(op.message));
    add_metadata_to_result(result);

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    return response.to_string();
}

std::string McpServer::handle_dtack(const std::string& id, const std::string& request) {
    std::string ack = extract_string_param(request, "ack");
    bool cancel = extract_bool_param(request, "cancel");

    if (ack.empty()) {
        return build_error(id, -32602, "Missing required parameter: ack");
    }

    log_operation("godot/dtack", "Polling ACK: " + ack + (cancel ? " (cancel)" : ""));

    std::lock_guard<std::mutex> lock(async_operations_mutex_);
    auto it = async_operations_.find(ack);

    if (it == async_operations_.end()) {
        return build_error(id, -32603, "Invalid or expired ACK token");
    }

    AsyncOperation& op = it->second;

    // Handle cancellation
    if (cancel) {
        if (op.cancel_callback) {
            op.cancel_callback();
        }
        op.status = "canceled";
        op.message = "Operation canceled";
    }

    // Build response
    JsonValue result = JsonValue::object();
    result.set("ack", JsonValue::string(ack));
    result.set("status", JsonValue::string(op.status));
    result.set("message", JsonValue::string(op.message));

    if (op.status == "complete" && !op.result_data.empty()) {
        result.set("data", JsonValue::string(op.result_data));
    }

    add_metadata_to_result(result);

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    // Remove from map if complete, error, or canceled
    if (op.status != "pending") {
        async_operations_.erase(it);
    }

    return response.to_string();
}

// Phase 1 Scene Manipulation Commands

std::string McpServer::handle_get_node_properties(const std::string& id, const std::string& request) {
    std::string node_path = extract_string_param(request, "node_path");

    if (node_path.empty()) {
        return build_error(id, -32602, "Missing required parameter: node_path");
    }

    log_operation("godot/get_node_properties", "Getting properties for: " + node_path);

    if (!get_node_properties_callback_) {
        return build_error(id, -32603, "get_node_properties callback not set");
    }

    // Call the callback (which will use ACK/DTACK pattern internally if needed)
    std::string properties_json = get_node_properties_callback_(node_path);

    if (properties_json.empty() || properties_json == "{}") {
        return build_error(id, -32603, "Node not found or has no properties: " + node_path);
    }

    // Parse the properties JSON and wrap in result
    JsonValue result = JsonValue::object();
    result.set("node_path", JsonValue::string(node_path));
    result.set("properties", JsonValue::string(properties_json));  // Raw JSON string
    add_metadata_to_result(result);

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    log_operation("godot/get_node_properties", "Retrieved properties for: " + node_path);
    return response.to_string();
}

std::string McpServer::handle_update_node_property(const std::string& id, const std::string& request) {
    std::string node_path = extract_string_param(request, "node_path");
    std::string property = extract_string_param(request, "property");
    std::string value = extract_string_param(request, "value");

    if (node_path.empty() || property.empty()) {
        return build_error(id, -32602, "Missing required parameters: node_path and property");
    }

    log_operation("godot/update_node_property", "Updating " + node_path + "." + property + " = " + value);

    if (!update_node_property_callback_) {
        return build_error(id, -32603, "update_node_property callback not set");
    }

    bool success = update_node_property_callback_(node_path, property, value);

    if (!success) {
        return build_error(id, -32603, "Failed to update property: " + property + " on node: " + node_path);
    }

    JsonValue result = JsonValue::object();
    result.set("success", JsonValue::boolean(true));
    result.set("node_path", JsonValue::string(node_path));
    result.set("property", JsonValue::string(property));
    result.set("value", JsonValue::string(value));
    add_metadata_to_result(result);

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    log_operation("godot/update_node_property", "Updated " + property + " on " + node_path);
    return response.to_string();
}

std::string McpServer::handle_duplicate_node(const std::string& id, const std::string& request) {
    std::string node_path = extract_string_param(request, "node_path");
    std::string new_name = extract_string_param(request, "new_name");

    if (node_path.empty()) {
        return build_error(id, -32602, "Missing required parameter: node_path");
    }

    log_operation("godot/duplicate_node", "Duplicating: " + node_path + " as " + new_name);

    if (!duplicate_node_callback_) {
        return build_error(id, -32603, "duplicate_node callback not set");
    }

    std::string new_node_path = duplicate_node_callback_(node_path, new_name);

    if (new_node_path.empty()) {
        return build_error(id, -32603, "Failed to duplicate node: " + node_path);
    }

    JsonValue result = JsonValue::object();
    result.set("success", JsonValue::boolean(true));
    result.set("original_path", JsonValue::string(node_path));
    result.set("new_path", JsonValue::string(new_node_path));
    add_metadata_to_result(result);

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    log_operation("godot/duplicate_node", "Duplicated to: " + new_node_path);
    return response.to_string();
}

std::string McpServer::handle_save_scene(const std::string& id, const std::string& request) {
    std::string path = extract_string_param(request, "path");

    log_operation("godot/save_scene", "Saving scene" + (path.empty() ? "" : " to: " + path));

    if (!save_scene_callback_) {
        return build_error(id, -32603, "save_scene callback not set");
    }

    std::string saved_path = save_scene_callback_(path);

    if (saved_path.empty()) {
        return build_error(id, -32603, "Failed to save scene");
    }

    JsonValue result = JsonValue::object();
    result.set("success", JsonValue::boolean(true));
    result.set("scene_path", JsonValue::string(saved_path));
    add_metadata_to_result(result);

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    log_operation("godot/save_scene", "Saved to: " + saved_path);
    return response.to_string();
}

std::string McpServer::handle_get_bounding_box(const std::string& id, const std::string& request) {
    std::string node_path = extract_string_param(request, "node_path");

    if (node_path.empty()) {
        return build_error(id, -32602, "Missing required parameter: node_path");
    }

    log_operation("godot/get_bounding_box", "Getting bounding box for: " + node_path);

    if (!get_bounding_box_callback_) {
        return build_error(id, -32603, "get_bounding_box callback not set");
    }

    std::string bbox_json = get_bounding_box_callback_(node_path);

    if (bbox_json.empty() || bbox_json == "{}") {
        return build_error(id, -32603, "Node not found or has no bounding box: " + node_path);
    }

    JsonValue result = JsonValue::object();
    result.set("node_path", JsonValue::string(node_path));
    result.set("bounding_box", JsonValue::string(bbox_json));
    add_metadata_to_result(result);

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    log_operation("godot/get_bounding_box", "Retrieved bounding box for: " + node_path);
    return response.to_string();
}

std::string McpServer::handle_get_selection(const std::string& id, const std::string& request) {
    log_operation("godot/get_selection", "Getting editor selection");

    if (!get_selection_callback_) {
        return build_error(id, -32603, "Selection callback not set");
    }

    std::string selection_json = get_selection_callback_();

    if (selection_json.empty()) {
        return build_error(id, -32603, "Failed to get selection");
    }

    JsonValue result = JsonValue::object();
    result.set("selection", JsonValue::string(selection_json));
    add_metadata_to_result(result);

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", JsonValue::string("2.0"));
    response.set("id", JsonValue::string(id));
    response.set("result", result);

    log_operation("godot/get_selection", "Retrieved editor selection");
    return response.to_string();
}

} // namespace mcp
