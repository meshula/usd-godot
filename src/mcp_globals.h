#ifndef MCP_GLOBALS_H
#define MCP_GLOBALS_H

#include <string>

namespace mcp {
    class McpServer;
    class McpHttpServer;
}

namespace usd_godot {
    // Get the global MCP server instance (may be nullptr)
    mcp::McpServer* get_mcp_server_instance();

    // Get the global HTTP MCP server instance (may be nullptr)
    mcp::McpHttpServer* get_mcp_http_server_instance();

    // Get/Set the global user notes (for real-time LLM communication)
    std::string get_user_notes();
    void set_user_notes(const std::string& notes);
}

#endif // MCP_GLOBALS_H
