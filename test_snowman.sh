#!/bin/bash
# Test script to create a snowman using MCP USD commands
# Creates 3 sphere prims (bottom, middle, head) and saves as snowman.usda

echo "========================================="
echo "Creating USD Snowman via MCP"
echo "========================================="
echo ""

cd test_project

# Start Godot and send multiple commands via stdin
(
    sleep 3
    echo "Sending create_stage command..."
    echo '{"jsonrpc":"2.0","id":"1","method":"usd/create_stage","params":{"file_path":"snowman.usda"}}'
    sleep 1

    echo "Sending create_prim for bottom sphere..."
    echo '{"jsonrpc":"2.0","id":"2","method":"usd/create_prim","params":{"stage_id":1,"prim_path":"/Snowman/Bottom","prim_type":"Sphere"}}'
    sleep 1

    echo "Sending create_prim for middle sphere..."
    echo '{"jsonrpc":"2.0","id":"3","method":"usd/create_prim","params":{"stage_id":1,"prim_path":"/Snowman/Middle","prim_type":"Sphere"}}'
    sleep 1

    echo "Sending create_prim for head sphere..."
    echo '{"jsonrpc":"2.0","id":"4","method":"usd/create_prim","params":{"stage_id":1,"prim_path":"/Snowman/Head","prim_type":"Sphere"}}'
    sleep 1

    echo "Sending query_generation command..."
    echo '{"jsonrpc":"2.0","id":"5","method":"usd/query_generation","params":{"stage_id":1}}'
    sleep 1

    echo "Sending save_stage command..."
    echo '{"jsonrpc":"2.0","id":"6","method":"usd/save_stage","params":{"stage_id":1,"file_path":"snowman.usda"}}'
    sleep 2

    pkill -9 godot
) | /Users/nporcino/dev/godot/bin/godot.macos.editor.arm64 -e --headless --mcp 2>&1 | \
    tee /tmp/snowman_test.log | \
    grep -E "(MCP Server|UsdStageManager|jsonrpc)"

echo ""
echo "========================================="
echo "Full output saved to: /tmp/snowman_test.log"
echo ""

if [ -f "snowman.usda" ]; then
    echo "✅ SUCCESS: snowman.usda created!"
    echo ""
    echo "Contents:"
    cat snowman.usda
else
    echo "❌ ERROR: snowman.usda not found"
fi

echo ""
echo "========================================="
