# MCP Server for Godot-USD Plugin

This document describes the MCP (Model Context Protocol) server implementation for the Godot-USD plugin.

## Quick Start

**To test the MCP handshake:**

```bash
cd /path/to/usd-godot
./test_working_handshake.sh
```

**To run Godot with MCP server:**

```bash
cd /path/to/project
/path/to/godot -e --headless --mcp
```

**Key requirement**: Use `-e` (editor mode) flag for GDExtensions to load. The `--script` mode will NOT work.

---

## Overview

The Godot-USD plugin now includes an MCP server that allows external tools to interact with the plugin through a standardized JSON-RPC 2.0 protocol over stdio.

## Architecture

### Components

1. **mcp_json.h** - Lightweight JSON builder without external dependencies
   - Supports JSON objects, arrays, strings, numbers, booleans, and null
   - Provides simple serialization to JSON strings

2. **mcp_server.h/cpp** - MCP server implementation
   - Runs in a background thread
   - Listens on stdin for JSON-RPC requests
   - Sends responses on stdout
   - Implements the MCP initialize handshake

3. **version.h** - Plugin version information
   - Defines plugin version constants (0.1.0)

4. **Integration in register_types.cpp**
   - Detects interactive mode via `--mcp` or `--interactive` command-line flags
   - Starts MCP server during MODULE_INITIALIZATION_LEVEL_SCENE
   - Stops server during uninitialize

## MCP Initialize Handshake

The server implements the MCP `initialize` method which reports:

- **Plugin Version**: `0.1.0`
- **Godot Version**: Extracted from `Engine::get_version_info()`
- **USD Version**: Extracted from `PXR_VERSION` macro (format: YY.MM)
- **Plugin Registration Status**: Whether USD plugins were successfully registered

### Example Initialize Request

```json
{
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
```

### Example Initialize Response

```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": {
    "protocolVersion": "2024-11-05",
    "capabilities": {},
    "serverInfo": {
      "name": "godot-usd",
      "version": "0.1.0"
    },
    "_meta": {
      "pluginVersion": "0.1.0",
      "godotVersion": "4.5.0",
      "usdVersion": "25.05",
      "pluginRegistered": true
    }
  }
}
```

## Running in Interactive Mode

To start the Godot-USD plugin with the MCP server:

### ✅ Working Method: Editor Mode (Headless)

```bash
cd /path/to/project
/path/to/godot -e --headless --mcp
```

**Important**:
- The `-e` flag (editor mode) is **required** for GDExtensions to load
- The `--mcp` or `--interactive` flag activates the MCP server
- GDExtensions do NOT load in `--script` mode, so avoid using that for MCP

### Expected Output

When the server starts successfully, you'll see:

```
USD-Godot: Initializing at SCENE level
USD: Registering plugins from: /path/to/project/lib/usd
USD: Registered 39 plugins
USD-Godot: Classes registered
USD-Godot: Interactive mode detected, starting MCP server
USD-Godot: MCP server started successfully
MCP Server: Started on stdio
```

## Testing the MCP Server

### ✅ Working Manual Test

The following command successfully tests the handshake:

```bash
cd /path/to/project

# Send initialize request and capture response
(sleep 3; echo '{"jsonrpc":"2.0","id":"1","method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}'; sleep 3; pkill -9 godot) | \
  /path/to/godot -e --headless --mcp 2>&1 | tail -30
```

**Expected Response:**

```
MCP Server: Initialize - Plugin: 0.1.0, Godot: 4.5.0, USD: 25.05, Registered: true
{"id":"1","jsonrpc":"2.0","result":{"_meta":{"godotVersion":"4.5.0","pluginRegistered":true,"pluginVersion":"0.1.0","usdVersion":"25.05"},"capabilities":{},"protocolVersion":"2024-11-05","serverInfo":{"name":"godot-usd","version":"0.1.0"}}}
```

### Test Script for Integration Testing

Create a test script that properly waits for initialization:

```bash
#!/bin/bash
cd /path/to/project

# Send request after Godot initializes (3 seconds)
(
    sleep 3
    echo '{"jsonrpc":"2.0","id":"1","method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}'
    sleep 2
) | /path/to/godot -e --headless --mcp 2>&1 | grep -A 2 "MCP Server"
```

## Implementation Details

### Thread Safety

- The MCP server runs in a background thread
- Uses `std::mutex` for I/O synchronization
- Safe startup and shutdown with atomic flags

### No External Dependencies

The implementation intentionally avoids third-party JSON libraries:
- Custom JSON builder in `mcp_json.h`
- Simple regex-free JSON parsing for extracting method names and IDs
- Standard C++ library only (string, thread, iostream, sstream, map, vector)

### Version Detection

- **Plugin Version**: Defined in `version.h` as `GODOT_USD_VERSION_STRING`
- **Godot Version**: `Engine::get_version_info()` provides major.minor.patch
- **USD Version**: `PXR_VERSION` macro from `pxr/pxr.h` (format: YYMM, e.g., 2505 = 25.05)
- **Registration Status**: Tracked via `s_usd_plugins_registered` flag

## Implemented USD Commands

The MCP server includes full USD stage management with generation tracking:

### Stage Management
- ✅ `usd/create_stage` - Create new USD stage (in-memory or file-based)
- ✅ `usd/save_stage` - Save stage to file
- ✅ `usd/query_generation` - Get generation number (tracks modifications)

### Prim Operations
- ✅ `usd/create_prim` - Create prim with type (e.g., Sphere, Xform)
- ✅ `usd/set_attribute` - Set an attribute on a prim
- ✅ `usd/get_attribute` - Get an attribute value from a prim
- ✅ `usd/set_transform` - Set transform (translation, rotation, scale) on a prim
- ✅ `usd/list_prims` - List all prims in a stage

### Generation Tracking
Every stage mutation increments a generation counter:
- Create prim → generation++
- Set attribute → generation++
- Set transform → generation++
- Save stage → generation stays same (saving doesn't modify)
- Query generation → check if saves are needed

### Tool Capabilities
The MCP initialize response now includes a complete list of all supported tools/commands:

```json
{
  "capabilities": {
    "tools": {
      "tools": [
        {"name": "usd/create_stage", "description": "Create a new USD stage (in-memory or file-based)"},
        {"name": "usd/save_stage", "description": "Save a USD stage to file"},
        {"name": "usd/query_generation", "description": "Query stage generation number (tracks modifications)"},
        {"name": "usd/create_prim", "description": "Create a prim with specified type"},
        {"name": "usd/set_attribute", "description": "Set an attribute on a prim"},
        {"name": "usd/get_attribute", "description": "Get an attribute value from a prim"},
        {"name": "usd/set_transform", "description": "Set transform (translation, rotation, scale) on a prim"},
        {"name": "usd/list_prims", "description": "List all prims in a stage"}
      ]
    }
  }
}
```

**Examples:**
```bash
./test_snowman.sh              # Creates basic 3-sphere snowman
./test_snowman_enhanced.sh     # Creates snowman with transforms and attributes
```

See [STAGE_MANAGER_IMPLEMENTATION.md](STAGE_MANAGER_IMPLEMENTATION.md) for full details.

## Future Enhancements

Additional capabilities to add:

1. **More USD Operations**
   - Create relationships
   - Query prim properties (type, metadata)

2. **Godot Resources**
   - `godot/scene_tree` - Get current scene tree structure
   - `godot/import_usd` - Import USD file into Godot

3. **Prompts**
   - USD stage inspection templates
   - Scene composition helpers

## Build Integration

The MCP server files are integrated into CMakeLists.txt:

```cmake
set(SOURCES
    ...
    src/mcp_server.cpp
    src/mcp_server.h
    src/mcp_json.h
    src/version.h
)
```

No additional build dependencies are required.

## Debugging

### Debug Output Messages

When running with the `--mcp` flag, look for these messages in order:

1. **Plugin Loading:**
   ```
   USD-Godot: Initializing at SCENE level
   USD: Registering plugins from: /path/to/project/lib/usd
   USD: Registered 39 plugins
   USD-Godot: Classes registered
   ```

2. **MCP Server Startup:**
   ```
   USD-Godot: Interactive mode detected, starting MCP server
   USD-Godot: MCP server started successfully
   MCP Server: Started on stdio
   ```

3. **Handshake Response:**
   ```
   MCP Server: Initialize - Plugin: X.X.X, Godot: X.X.X, USD: XX.XX, Registered: true/false
   ```

### Common Issues

**Problem**: No C++ output, only GDScript messages
- **Cause**: Running in `--script` mode instead of `-e` (editor mode)
- **Solution**: Use `-e --headless --mcp` flags together

**Problem**: "USD-Godot: Not in interactive mode"
- **Cause**: Missing `--mcp` or `--interactive` flag
- **Solution**: Add the flag to the command line

**Problem**: No handshake response
- **Cause**: Godot still initializing when request sent
- **Solution**: Wait 3+ seconds after launch before sending request

## References

- [Model Context Protocol Specification](https://modelcontextprotocol.io/)
- [JSON-RPC 2.0 Specification](https://www.jsonrpc.org/specification)
- [Pixar USD Documentation](https://openusd.org/)
- [Godot GDExtension Documentation](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/index.html)
