#ifndef USD_GODOT_MCP_SERVER_H
#define USD_GODOT_MCP_SERVER_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <functional>
#include <map>

namespace mcp {

// Forward declaration
class JsonValue;

class McpServer {
public:
    // Callback for logging operations to control panel
    using LogCallback = std::function<void(const std::string&, const std::string&)>;

    // Callback for importing USD to scene group (returns node count on success, -1 on failure)
    using ImportCallback = std::function<int(const std::string& file_path, const std::string& group_name, bool force)>;

    // Callback for querying scene tree (returns JSON string with node data)
    using QuerySceneCallback = std::function<std::string(const std::string& path)>;

    // Callbacks for Phase 1 scene manipulation commands
    using GetNodePropertiesCallback = std::function<std::string(const std::string& node_path)>;
    using UpdateNodePropertyCallback = std::function<bool(const std::string& node_path, const std::string& property, const std::string& value)>;
    using DuplicateNodeCallback = std::function<std::string(const std::string& node_path, const std::string& new_name)>;
    using SaveSceneCallback = std::function<std::string(const std::string& path)>;
    using GetBoundingBoxCallback = std::function<std::string(const std::string& node_path)>;
    using GetSelectionCallback = std::function<std::string()>;

    McpServer();
    ~McpServer();

    // Start the MCP server (in a background thread)
    bool start();

    // Stop the MCP server
    void stop();

    // Check if the server is running
    bool is_running() const { return running_; }

    // Get plugin registration status
    void set_plugin_registered(bool registered) { plugin_registered_ = registered; }

    // Set callback for operation logging
    void set_log_callback(LogCallback callback) { log_callback_ = callback; }

    // Set callback for importing USD to scene
    void set_import_callback(ImportCallback callback) { import_callback_ = callback; }

    // Set callback for querying scene tree
    void set_query_scene_callback(QuerySceneCallback callback) { query_scene_callback_ = callback; }

    // Set callbacks for Phase 1 scene manipulation commands
    void set_get_node_properties_callback(GetNodePropertiesCallback callback) { get_node_properties_callback_ = callback; }
    void set_update_node_property_callback(UpdateNodePropertyCallback callback) { update_node_property_callback_ = callback; }
    void set_duplicate_node_callback(DuplicateNodeCallback callback) { duplicate_node_callback_ = callback; }
    void set_save_scene_callback(SaveSceneCallback callback) { save_scene_callback_ = callback; }
    void set_get_bounding_box_callback(GetBoundingBoxCallback callback) { get_bounding_box_callback_ = callback; }
    void set_get_selection_callback(GetSelectionCallback callback) { get_selection_callback_ = callback; }

    // Process a JSON-RPC request and return the response
    // This is exposed for HTTP transport mode
    std::string process_request_sync(const std::string& request);

private:
    // Main server loop (runs in background thread)
    void run();

    // Process a single JSON-RPC request
    void process_request(const std::string& request);

    // Handle the MCP initialize request
    std::string handle_initialize(const std::string& id);

    // Handle USD stage management commands
    std::string handle_create_stage(const std::string& id, const std::string& request);
    std::string handle_save_stage(const std::string& id, const std::string& request);
    std::string handle_query_generation(const std::string& id, const std::string& request);
    std::string handle_create_prim(const std::string& id, const std::string& request);
    std::string handle_set_attribute(const std::string& id, const std::string& request);
    std::string handle_get_attribute(const std::string& id, const std::string& request);
    std::string handle_set_transform(const std::string& id, const std::string& request);
    std::string handle_list_prims(const std::string& id, const std::string& request);

    // Handle USD Stage Manager Panel commands (Phase 4)
    std::string handle_list_stages(const std::string& id, const std::string& request);
    std::string handle_create_scene_group(const std::string& id, const std::string& request);
    std::string handle_reflect_to_scene(const std::string& id, const std::string& request);
    std::string handle_confirm_reflect(const std::string& id, const std::string& request);

    // Handle Godot scene tree query (ACK/DTACK pattern)
    std::string handle_query_scene_tree(const std::string& id, const std::string& request);

    // Handle DTACK polling for async operations
    std::string handle_dtack(const std::string& id, const std::string& request);

    // Handle Godot scene manipulation commands (Phase 1)
    std::string handle_get_node_properties(const std::string& id, const std::string& request);
    std::string handle_update_node_property(const std::string& id, const std::string& request);
    std::string handle_duplicate_node(const std::string& id, const std::string& request);
    std::string handle_save_scene(const std::string& id, const std::string& request);
    std::string handle_get_bounding_box(const std::string& id, const std::string& request);
    std::string handle_get_selection(const std::string& id, const std::string& request);

    // Send a JSON-RPC response
    void send_response(const std::string& response);

    // Helper to build error response
    std::string build_error(const std::string& id, int code, const std::string& message);

    // Helper to add metadata (including user notes) to result
    void add_metadata_to_result(JsonValue& result);

    // Helper to log operations (if callback is set)
    void log_operation(const std::string& operation, const std::string& details = "");

    // Parse a simple JSON request to extract method name and parameters
    // Returns empty string if parsing fails
    std::string extract_method(const std::string& request);
    std::string extract_id(const std::string& request);
    std::string extract_string_param(const std::string& request, const std::string& param_name);
    int64_t extract_int_param(const std::string& request, const std::string& param_name);
    double extract_double_param(const std::string& request, const std::string& param_name);
    bool extract_bool_param(const std::string& request, const std::string& param_name);

    std::thread server_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    bool plugin_registered_;
    std::mutex io_mutex_;
    LogCallback log_callback_;
    ImportCallback import_callback_;
    QuerySceneCallback query_scene_callback_;
    GetNodePropertiesCallback get_node_properties_callback_;
    UpdateNodePropertyCallback update_node_property_callback_;
    DuplicateNodeCallback duplicate_node_callback_;
    SaveSceneCallback save_scene_callback_;
    GetBoundingBoxCallback get_bounding_box_callback_;
    GetSelectionCallback get_selection_callback_;

    // Confirmation tokens for reflect operations (Phase 4)
    struct ReflectConfirmation {
        std::string file_path;
        std::string group_name;
    };
    std::map<std::string, ReflectConfirmation> pending_confirmations_;
    std::mutex confirmations_mutex_;

    // ACK/DTACK async operation tracking
    struct AsyncOperation {
        std::string ack_token;
        std::string status;  // "pending", "complete", "error", "canceled"
        std::string message;
        std::string result_data;  // JSON string with actual results
        std::function<void()> cancel_callback;  // Optional cancellation
    };
    std::map<std::string, AsyncOperation> async_operations_;
    std::mutex async_operations_mutex_;

    // Helper to generate ACK tokens
    std::string generate_ack_token();
};

} // namespace mcp

#endif // USD_GODOT_MCP_SERVER_H
