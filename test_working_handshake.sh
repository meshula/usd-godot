#!/bin/bash
# Working test script for MCP handshake
# This script successfully tests the initialize handshake

echo "========================================="
echo "Testing MCP Handshake with Godot-USD"
echo "========================================="
echo ""

cd test_project

echo "Starting Godot in editor mode with MCP server..."
echo "Waiting 3 seconds for initialization..."
echo ""

# Send initialize request after 3 seconds, capture response
(
    sleep 3
    echo '{"jsonrpc":"2.0","id":"1","method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}'
    sleep 3
    pkill -9 godot
) | /Users/nporcino/dev/godot/bin/godot.macos.editor.arm64 -e --headless --mcp 2>&1 | \
    tee /tmp/mcp_test_output.log | \
    grep -E "(USD-Godot|MCP Server|jsonrpc)"

echo ""
echo "========================================="
echo "Full output saved to: /tmp/mcp_test_output.log"
echo "========================================="
