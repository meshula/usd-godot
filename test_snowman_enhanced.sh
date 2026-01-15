#!/bin/bash

# Enhanced test that creates a snowman with proper transforms and attributes
# Tests: create_stage, create_prim, set_transform, set_attribute, list_prims, save_stage

GODOT_PATH="/Users/nporcino/dev/godot/bin/godot.macos.editor.arm64"
PROJECT_PATH="/Users/nporcino/dev/Lab/usd-godot/test_project"

echo "Starting enhanced snowman test with transforms..."

(
    # Wait for initialization
    sleep 3

    # Create stage
    echo '{"jsonrpc":"2.0","id":"1","method":"usd/create_stage","params":{"file_path":""}}'
    sleep 0.5

    # Create Xform root
    echo '{"jsonrpc":"2.0","id":"2","method":"usd/create_prim","params":{"stage_id":1,"prim_path":"/Snowman","prim_type":"Xform"}}'
    sleep 0.5

    # Create bottom sphere (largest)
    echo '{"jsonrpc":"2.0","id":"3","method":"usd/create_prim","params":{"stage_id":1,"prim_path":"/Snowman/Bottom","prim_type":"Sphere"}}'
    sleep 0.5

    # Set transform for bottom sphere (at origin, scale 1.5)
    echo '{"jsonrpc":"2.0","id":"4","method":"usd/set_transform","params":{"stage_id":1,"prim_path":"/Snowman/Bottom","tx":0,"ty":0,"tz":0,"rx":0,"ry":0,"rz":0,"sx":1.5,"sy":1.5,"sz":1.5}}'
    sleep 0.5

    # Set radius attribute for bottom sphere
    echo '{"jsonrpc":"2.0","id":"5","method":"usd/set_attribute","params":{"stage_id":1,"prim_path":"/Snowman/Bottom","attr_name":"radius","value_type":"double","value":"1.5"}}'
    sleep 0.5

    # Create middle sphere
    echo '{"jsonrpc":"2.0","id":"6","method":"usd/create_prim","params":{"stage_id":1,"prim_path":"/Snowman/Middle","prim_type":"Sphere"}}'
    sleep 0.5

    # Set transform for middle sphere (positioned above bottom, scale 1.0)
    echo '{"jsonrpc":"2.0","id":"7","method":"usd/set_transform","params":{"stage_id":1,"prim_path":"/Snowman/Middle","tx":0,"ty":3.0,"tz":0,"rx":0,"ry":0,"rz":0,"sx":1.0,"sy":1.0,"sz":1.0}}'
    sleep 0.5

    # Set radius attribute for middle sphere
    echo '{"jsonrpc":"2.0","id":"8","method":"usd/set_attribute","params":{"stage_id":1,"prim_path":"/Snowman/Middle","attr_name":"radius","value_type":"double","value":"1.0"}}'
    sleep 0.5

    # Create top sphere (head, smallest)
    echo '{"jsonrpc":"2.0","id":"9","method":"usd/create_prim","params":{"stage_id":1,"prim_path":"/Snowman/Head","prim_type":"Sphere"}}'
    sleep 0.5

    # Set transform for head sphere (positioned above middle, scale 0.7)
    echo '{"jsonrpc":"2.0","id":"10","method":"usd/set_transform","params":{"stage_id":1,"prim_path":"/Snowman/Head","tx":0,"ty":5.5,"tz":0,"rx":0,"ry":0,"rz":0,"sx":0.7,"sy":0.7,"sz":0.7}}'
    sleep 0.5

    # Set radius attribute for head sphere
    echo '{"jsonrpc":"2.0","id":"11","method":"usd/set_attribute","params":{"stage_id":1,"prim_path":"/Snowman/Head","attr_name":"radius","value_type":"double","value":"0.7"}}'
    sleep 0.5

    # List all prims
    echo '{"jsonrpc":"2.0","id":"12","method":"usd/list_prims","params":{"stage_id":1}}'
    sleep 0.5

    # Query generation (should be high from all the modifications)
    echo '{"jsonrpc":"2.0","id":"13","method":"usd/query_generation","params":{"stage_id":1}}'
    sleep 0.5

    # Get attribute back to verify
    echo '{"jsonrpc":"2.0","id":"14","method":"usd/get_attribute","params":{"stage_id":1,"prim_path":"/Snowman/Bottom","attr_name":"radius"}}'
    sleep 0.5

    # Save to file (use absolute path)
    echo '{"jsonrpc":"2.0","id":"15","method":"usd/save_stage","params":{"stage_id":1,"file_path":"/Users/nporcino/dev/Lab/usd-godot/test_project/snowman_enhanced.usda"}}'
    sleep 1

    # Kill Godot
    pkill -9 godot

) | $GODOT_PATH -e --headless --mcp --path $PROJECT_PATH 2>&1 | grep -E "(MCP Server|stage_id|generation|prims|count|radius|success)"

echo ""
echo "Enhanced test complete! Check test_project/snowman_enhanced.usda"
