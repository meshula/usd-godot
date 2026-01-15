#ifndef USD_GODOT_MCP_SERVER_H
#define USD_GODOT_MCP_SERVER_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>

namespace mcp {

class McpServer {
public:
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

    // Send a JSON-RPC response
    void send_response(const std::string& response);

    // Helper to send error response
    void send_error(const std::string& id, int code, const std::string& message);

    // Parse a simple JSON request to extract method name and parameters
    // Returns empty string if parsing fails
    std::string extract_method(const std::string& request);
    std::string extract_id(const std::string& request);
    std::string extract_string_param(const std::string& request, const std::string& param_name);
    int64_t extract_int_param(const std::string& request, const std::string& param_name);

    std::thread server_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    bool plugin_registered_;
    std::mutex io_mutex_;
};

} // namespace mcp

#endif // USD_GODOT_MCP_SERVER_H
