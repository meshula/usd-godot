#!/bin/bash

# Simple test script for MCP handshake

echo "Starting Godot with MCP server..."
echo ""

# Send initialize request after a brief delay
(
    sleep 2
    echo '{"jsonrpc":"2.0","id":"1","method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}'
    sleep 3
) | /Users/nporcino/dev/godot/bin/godot.macos.editor.arm64 \
    --headless \
    --mcp \
    --script /Users/nporcino/dev/Lab/usd-godot/test_project/test_mcp.gd \
    --path /Users/nporcino/dev/Lab/usd-godot/test_project \
    2>&1
