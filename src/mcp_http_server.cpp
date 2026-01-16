#include "mcp_http_server.h"
#include "mcp_server.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/ip.hpp>
#include <godot_cpp/variant/array.hpp>
#include <sstream>
#include <chrono>
#include <thread>

using namespace godot;

namespace mcp {

McpHttpServer::McpHttpServer()
    : running_(false)
    , port_(3000)
    , mcp_server_(nullptr) {
}

McpHttpServer::~McpHttpServer() {
    stop();
}

void McpHttpServer::set_log_callback(LogCallback callback) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_callback_ = callback;
}

void McpHttpServer::log_operation(const std::string& operation, const std::string& details) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (log_callback_) {
        log_callback_(operation, details);
    }
}

bool McpHttpServer::start(int port) {
    if (running_) {
        return false;
    }

    port_ = port;
    tcp_server_.instantiate();

    Error err = tcp_server_->listen(port_, "127.0.0.1");
    if (err != OK) {
        UtilityFunctions::print("MCP HTTP Server: Failed to listen on port ", port_, " error: ", (int)err);
        return false;
    }

    running_ = true;
    server_thread_ = std::thread(&McpHttpServer::run, this);

    UtilityFunctions::print("MCP HTTP Server: Started on http://127.0.0.1:", port_);
    log_operation("HTTP Server Started", "Port: " + std::to_string(port_));
    return true;
}

void McpHttpServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Close all SSE clients
    {
        std::lock_guard<std::mutex> lock(sse_clients_mutex_);
        for (auto& client : sse_clients_) {
            if (client.is_valid() && client->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
                client->disconnect_from_host();
            }
        }
        sse_clients_.clear();
    }

    // Stop TCP server
    if (tcp_server_.is_valid() && tcp_server_->is_listening()) {
        tcp_server_->stop();
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    UtilityFunctions::print("MCP HTTP Server: Stopped");
    log_operation("HTTP Server Stopped", "");
}

void McpHttpServer::run() {
    while (running_) {
        poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    UtilityFunctions::print("MCP HTTP Server: Thread exiting cleanly");
}

void McpHttpServer::poll() {
    if (!tcp_server_.is_valid() || !tcp_server_->is_listening()) {
        return;
    }

    // Accept new connections
    if (tcp_server_->is_connection_available()) {
        Ref<StreamPeerTCP> client = tcp_server_->take_connection();
        if (client.is_valid()) {
            // Handle client in blocking mode (we're already in a background thread)
            handle_client(client);
        }
    }

    // Clean up disconnected SSE clients
    {
        std::lock_guard<std::mutex> lock(sse_clients_mutex_);
        sse_clients_.erase(
            std::remove_if(sse_clients_.begin(), sse_clients_.end(),
                [](const Ref<StreamPeerTCP>& client) {
                    return !client.is_valid() || client->get_status() != StreamPeerTCP::STATUS_CONNECTED;
                }),
            sse_clients_.end()
        );
    }
}

void McpHttpServer::handle_client(Ref<StreamPeerTCP> client) {
    if (!client.is_valid()) {
        return;
    }

    // Read HTTP request with timeout (5 seconds)
    std::string raw_request;
    const int timeout_ms = 5000;
    auto start_time = std::chrono::steady_clock::now();

    while (client->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
        int available = client->get_available_bytes();
        if (available > 0) {
            Array result = client->get_partial_data(available);
            Error err = (Error)(int)result[0];
            if (err == OK) {
                PackedByteArray data = result[1];
                if (data.size() > 0) {
                    raw_request.append(reinterpret_cast<const char*>(data.ptr()), data.size());

                    // Check if we have complete headers (double CRLF)
                    if (raw_request.find("\r\n\r\n") != std::string::npos) {
                        break;
                    }
                }
            }
        }

        // Check timeout
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        if (elapsed > timeout_ms) {
            UtilityFunctions::print("MCP HTTP Server: Request timeout");
            client->disconnect_from_host();
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (raw_request.empty()) {
        client->disconnect_from_host();
        return;
    }

    // Parse HTTP request
    HttpRequest request = parse_http_request(raw_request);
    if (!request.valid) {
        send_http_response(client, 400, "Bad Request", {}, "Invalid HTTP request");
        client->disconnect_from_host();
        return;
    }

    // Route request
    if (request.method == "POST" && request.path == "/message") {
        std::string response_body = handle_message_endpoint(request);
        std::map<std::string, std::string> headers;
        headers["Content-Type"] = "application/json";
        headers["Access-Control-Allow-Origin"] = "*";
        send_http_response(client, 200, "OK", headers, response_body);
        client->disconnect_from_host();
    } else if ((request.method == "POST" || request.method == "GET") && request.path == "/sse") {
        // SSE endpoint - keep connection alive (accept both GET and POST)
        handle_sse_endpoint(client, request);
    } else if (request.method == "OPTIONS") {
        // CORS preflight
        std::map<std::string, std::string> headers;
        headers["Access-Control-Allow-Origin"] = "*";
        headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
        headers["Access-Control-Allow-Headers"] = "Content-Type";
        send_http_response(client, 204, "No Content", headers, "");
        client->disconnect_from_host();
    } else {
        send_http_response(client, 404, "Not Found", {}, "Endpoint not found");
        client->disconnect_from_host();
    }
}

McpHttpServer::HttpRequest McpHttpServer::parse_http_request(const std::string& raw_request) {
    HttpRequest request;

    size_t header_end = raw_request.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return request; // Invalid
    }

    std::string header_section = raw_request.substr(0, header_end);
    std::istringstream stream(header_section);
    std::string line;

    // Parse request line
    if (std::getline(stream, line)) {
        // Remove \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream request_line(line);
        std::string version;
        request_line >> request.method >> request.path >> version;
    }

    // Parse headers
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            // Trim leading space from value
            if (!value.empty() && value[0] == ' ') {
                value = value.substr(1);
            }
            request.headers[key] = value;
        }
    }

    // Extract body if present
    if (header_end + 4 < raw_request.size()) {
        request.body = raw_request.substr(header_end + 4);
    }

    request.valid = !request.method.empty() && !request.path.empty();
    return request;
}

std::string McpHttpServer::handle_message_endpoint(const HttpRequest& request) {
    // Delegate to MCP server's process_request
    if (!mcp_server_) {
        return R"({"jsonrpc":"2.0","error":{"code":-32603,"message":"MCP server not initialized"},"id":null})";
    }

    log_operation("HTTP Request", request.path + " - " + std::to_string(request.body.size()) + " bytes");

    // Process the JSON-RPC request synchronously and return response
    std::string response = mcp_server_->process_request_sync(request.body);
    return response;
}

void McpHttpServer::handle_sse_endpoint(Ref<StreamPeerTCP> client, const HttpRequest& request) {
    // Send SSE headers
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "text/event-stream";
    headers["Cache-Control"] = "no-cache";
    headers["Connection"] = "keep-alive";
    headers["Access-Control-Allow-Origin"] = "*";

    // Send headers manually (don't close connection)
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    for (const auto& [key, value] : headers) {
        response << key << ": " << value << "\r\n";
    }
    response << "\r\n";

    std::string response_str = response.str();
    PackedByteArray data;
    data.resize(response_str.size());
    memcpy(data.ptrw(), response_str.c_str(), response_str.size());
    client->put_data(data);

    // Send initial comment to keep connection alive
    std::string keepalive = ": keepalive\n\n";
    PackedByteArray keepalive_data;
    keepalive_data.resize(keepalive.size());
    memcpy(keepalive_data.ptrw(), keepalive.c_str(), keepalive.size());
    client->put_data(keepalive_data);

    // Add to SSE clients list
    {
        std::lock_guard<std::mutex> lock(sse_clients_mutex_);
        sse_clients_.push_back(client);
    }

    log_operation("SSE Connection", "Client connected");
    UtilityFunctions::print("MCP HTTP Server: SSE client connected");
}

void McpHttpServer::send_http_response(Ref<StreamPeerTCP> client,
                                       int status_code,
                                       const std::string& status_text,
                                       const std::map<std::string, std::string>& headers,
                                       const std::string& body) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";

    // Add headers
    for (const auto& [key, value] : headers) {
        response << key << ": " << value << "\r\n";
    }

    // Add Content-Length
    response << "Content-Length: " << body.size() << "\r\n";
    response << "\r\n";

    // Add body
    response << body;

    std::string response_str = response.str();
    PackedByteArray data;
    data.resize(response_str.size());
    memcpy(data.ptrw(), response_str.c_str(), response_str.size());

    client->put_data(data);
}

void McpHttpServer::send_sse_event(const std::string& event_type, const std::string& data) {
    std::lock_guard<std::mutex> lock(sse_clients_mutex_);

    std::ostringstream event;
    event << "event: " << event_type << "\n";
    event << "data: " << data << "\n\n";

    std::string event_str = event.str();
    PackedByteArray event_data;
    event_data.resize(event_str.size());
    memcpy(event_data.ptrw(), event_str.c_str(), event_str.size());

    for (auto& client : sse_clients_) {
        if (client.is_valid() && client->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
            client->put_data(event_data);
        }
    }
}

} // namespace mcp
