#ifndef USD_GODOT_MCP_SERVER_H
#define USD_GODOT_MCP_SERVER_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <functional>

namespace mcp {

// Forward declaration
class JsonValue;

class McpServer {
public:
    // Callback for logging operations to control panel
    using LogCallback = std::function<void(const std::string&, const std::string&)>;

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

    std::thread server_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    bool plugin_registered_;
    std::mutex io_mutex_;
    LogCallback log_callback_;
};

} // namespace mcp

#endif // USD_GODOT_MCP_SERVER_H
