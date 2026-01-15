#!/bin/bash
cd /Users/nporcino/dev/Lab/usd-godot/test_project

# Start Godot in background
/Users/nporcino/dev/godot/bin/godot.macos.editor.arm64 -e --headless --mcp 2>&1 &
GODOT_PID=$!

# Give it time to initialize
sleep 3

# Send initialize request
echo '{"jsonrpc":"2.0","id":"1","method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}' > /tmp/mcp_request.json
cat /tmp/mcp_request.json | nc localhost 12345 2>/dev/null || echo "Using stdin..."

# Wait a bit for response
sleep 2

# Kill Godot
kill $GODOT_PID 2>/dev/null
wait $GODOT_PID 2>/dev/null

echo "Test complete"
