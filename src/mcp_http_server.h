#ifndef USD_GODOT_MCP_HTTP_SERVER_H
#define USD_GODOT_MCP_HTTP_SERVER_H

#include <godot_cpp/classes/tcp_server.hpp>
#include <godot_cpp/classes/stream_peer_tcp.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <map>
#include <functional>

namespace mcp {

// Forward declare McpServer to access its handlers
class McpServer;

/**
 * HTTP transport for MCP protocol using Server-Sent Events (SSE)
 *
 * Implements the MCP HTTP+SSE transport specification:
 * - POST /sse - Establish SSE connection for server->client messages
 * - POST /message - Send client->server JSON-RPC requests
 *
 * This allows Godot to run as an HTTP server that Claude Code can connect to,
 * enabling the GUI and MCP to coexist.
 */
class McpHttpServer {
public:
    using LogCallback = std::function<void(const std::string&, const std::string&)>;

    McpHttpServer();
    ~McpHttpServer();

    // Start HTTP server on specified port
    bool start(int port = 3000);

    // Stop HTTP server
    void stop();

    // Check if server is running
    bool is_running() const { return running_; }

    // Get the port we're listening on
    int get_port() const { return port_; }

    // Set callback for operation logging
    void set_log_callback(LogCallback callback);

    // Set the MCP server instance to delegate JSON-RPC handling
    void set_mcp_server(McpServer* server) { mcp_server_ = server; }

private:
    // Main server loop (runs in background thread)
    void run();

    // Poll for new connections and handle existing ones
    void poll();

    // Handle a single client connection
    void handle_client(godot::Ref<godot::StreamPeerTCP> client);

    // Parse HTTP request and extract method, path, headers, body
    struct HttpRequest {
        std::string method;
        std::string path;
        std::map<std::string, std::string> headers;
        std::string body;
        bool valid = false;
    };
    HttpRequest parse_http_request(const std::string& raw_request);

    // Handle POST /message endpoint
    std::string handle_message_endpoint(const HttpRequest& request);

    // Handle POST /sse endpoint
    void handle_sse_endpoint(godot::Ref<godot::StreamPeerTCP> client, const HttpRequest& request);

    // Send HTTP response
    void send_http_response(godot::Ref<godot::StreamPeerTCP> client,
                           int status_code,
                           const std::string& status_text,
                           const std::map<std::string, std::string>& headers,
                           const std::string& body);

    // Send SSE event to all connected clients
    void send_sse_event(const std::string& event_type, const std::string& data);

    // Helper to log operations
    void log_operation(const std::string& operation, const std::string& details = "");

    godot::Ref<godot::TCPServer> tcp_server_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    int port_;

    // SSE connections (kept alive for server->client messages)
    std::mutex sse_clients_mutex_;
    std::vector<godot::Ref<godot::StreamPeerTCP>> sse_clients_;

    // MCP server for delegating JSON-RPC requests
    McpServer* mcp_server_;

    LogCallback log_callback_;
    std::mutex log_mutex_;
};

} // namespace mcp

#endif // USD_GODOT_MCP_HTTP_SERVER_H
