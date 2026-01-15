#!/usr/bin/env python3
"""
Simple test script to verify MCP handshake with Godot-USD plugin
"""

import subprocess
import json
import sys
import time

def test_mcp_handshake():
    """Test the MCP initialize handshake"""

    # Start Godot in interactive mode with the plugin
    godot_cmd = [
        "/Users/nporcino/dev/godot/bin/godot.macos.editor.arm64",
        "--headless",
        "--mcp",
        "--path", "/Users/nporcino/dev/Lab/usd-godot/test_project"
    ]

    print("Starting Godot with MCP server...")
    print(f"Command: {' '.join(godot_cmd)}")
    print()

    try:
        proc = subprocess.Popen(
            godot_cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )

        # Give it a moment to start
        time.sleep(2)

        # Send initialize request
        initialize_request = {
            "jsonrpc": "2.0",
            "id": "1",
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {
                    "name": "test-client",
                    "version": "1.0.0"
                }
            }
        }

        request_json = json.dumps(initialize_request)
        print(f"Sending initialize request:")
        print(request_json)
        print()

        # Send request
        proc.stdin.write(request_json + "\n")
        proc.stdin.flush()

        # Read response
        print("Waiting for response...")
        response_line = proc.stdout.readline()

        if response_line:
            print(f"Received response:")
            print(response_line)
            print()

            try:
                response = json.loads(response_line)

                # Print version information
                if "result" in response:
                    result = response["result"]

                    print("=== MCP Handshake Successful ===")
                    print(f"Protocol Version: {result.get('protocolVersion', 'N/A')}")

                    if "serverInfo" in result:
                        print(f"Server Name: {result['serverInfo'].get('name', 'N/A')}")
                        print(f"Server Version: {result['serverInfo'].get('version', 'N/A')}")

                    if "_meta" in result:
                        meta = result["_meta"]
                        print("\n=== Version Information ===")
                        print(f"Plugin Version: {meta.get('pluginVersion', 'N/A')}")
                        print(f"Godot Version: {meta.get('godotVersion', 'N/A')}")
                        print(f"USD Version: {meta.get('usdVersion', 'N/A')}")
                        print(f"Plugin Registered: {meta.get('pluginRegistered', 'N/A')}")
                else:
                    print("Error: No result in response")
                    if "error" in response:
                        print(f"Error: {response['error']}")

            except json.JSONDecodeError as e:
                print(f"Failed to parse response as JSON: {e}")
        else:
            print("No response received")

        # Check stderr for any errors
        time.sleep(1)
        stderr_output = proc.stderr.read()
        if stderr_output:
            print("\n=== Stderr Output ===")
            print(stderr_output)

        # Clean up
        proc.terminate()
        proc.wait(timeout=5)

    except Exception as e:
        print(f"Error: {e}")
        return False

    return True

if __name__ == "__main__":
    success = test_mcp_handshake()
    sys.exit(0 if success else 1)
