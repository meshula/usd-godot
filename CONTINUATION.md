# USD-Godot MCP Implementation - Session Continuation

## Current Status (January 15, 2026)

### ✅ Phase 1 Commands - COMPLETE

All Phase 1 MCP commands from `MCP_COMMANDS_PLAN.md` are now implemented and tested:

1. **godot/query_scene_tree** - Query scene hierarchy (ACK/DTACK pattern)
2. **godot/dtack** - Poll async operation status
3. **godot/get_node_properties** - Get all properties of a node
4. **godot/update_node_property** - Update a single property on a node
5. **godot/duplicate_node** - Duplicate node with full hierarchy (DEEP COPY)
6. **godot/save_scene** - Save the current scene
7. **godot/get_bounding_box** - Get spatial bounds of a node

**Progress**: 7/40+ commands implemented (17.5%)

---

## What Was Accomplished

### Deep Copy Fix (Critical)
Fixed the shallow copy issue in `godot/duplicate_node`:
- Added `Node::DUPLICATE_USE_INSTANTIATION` flag to copy entire hierarchy
- Implemented recursive owner assignment for all children
- **Result**: Duplicated nodes now include complete child hierarchy

**Files Modified**:
- `src/usd_plugin.cpp` (lines 1235-1270)

### Thread Safety Implementation
All Phase 1 commands use thread-safe ACK/DTACK-style synchronization:
- Request structures with mutex/condition_variable
- Pending operation maps with shared_ptr
- `call_deferred` for main thread execution
- 5-second timeout to prevent hangs

**Pattern Example**:
```cpp
auto request = std::make_shared<DuplicateNodeRequest>();
request->node_path = String(node_path.c_str());
request->new_name = String(new_name.c_str());

int op_id;
{
    std::lock_guard<std::mutex> lock(pending_operations_mutex_);
    op_id = next_operation_id_++;
    pending_duplicate_node_[op_id] = request;
}

call_deferred("_perform_duplicate_node_deferred", op_id);

std::unique_lock<std::mutex> lock(request->mutex);
if (request->cv.wait_for(lock, std::chrono::seconds(5), [&request]{ return request->done; })) {
    return request->result;
}
```

### Bounding Box Command
Implemented `godot/get_bounding_box` for spatial awareness:
- Recursively collects AABBs from all VisualInstance3D children
- Transforms to global space and merges
- Returns JSON with min, max, center, size

**Use Case**: Enables precise positioning based on actual geometry dimensions.

### Testing Results
Successfully tested complete workflow:
1. ✅ Duplicated formula_car to create car2 and car3 (with full hierarchies)
2. ✅ Positioned cars using bounding box data (303.78 unit spacing, 50 unit gaps)
3. ✅ Saved scene with all changes persisted

**Test Scene**: `/Users/nporcino/dev/Lab/usd-godot/test_project/test_scene.tscn`
- 3 formula cars positioned on starting line
- Car width: 253.78 units
- Spacing: 303.78 units between pivot points (50 unit gaps)

---

## MCP Server Configuration

### Current Setup
**Server**: Running in Godot editor on `http://127.0.0.1:3000`
**Config**: `.mcp.json` exists at project root:
```json
{
  "mcpServers": {
    "usd-godot": {
      "url": "http://127.0.0.1:3000"
    }
  }
}
```

### Using the MCP

**Option 1: Native MCP Integration (Recommended)**
1. Restart Claude Code to load `.mcp.json`
2. MCP tools will be available as native functions
3. Example call:
```
Use godot/duplicate_node tool with:
  node_path: "formula_car"
  new_name: "car4"
```

**Option 2: Direct curl (Current Method)**
```bash
curl -X POST http://127.0.0.1:3000/message -H "Content-Type: application/json" -d '{
  "jsonrpc":"2.0",
  "id":"1",
  "method":"godot/duplicate_node",
  "params":{"node_path":"formula_car","new_name":"car4"}
}'
```

### Important Notes

**Path Format**:
- Use **relative paths** without leading slash: `"formula_car"`, `"car2"`
- NOT: `"/formula_car"` (will fail)

**Position Format**:
- Vector3 as JSON array string: `"[x, y, z]"`
- Example: `"[-303.78, 0.0, 0.0]"`

**Property Types**:
The `update_node_property` command auto-detects types:
- `position`, `rotation`, `scale` → Vector3
- Numeric strings → int or float
- `"true"`/`"false"` → bool
- Everything else → String

---

## File Changes Summary

### New Handler Methods
**src/mcp_server.h** (lines 29-34, 60-64, 102-107):
- Added callback typedefs for Phase 1 commands
- Added handler method declarations
- Added callback member variables

**src/mcp_server.cpp** (lines 1391-1566):
- Implemented 5 new command handlers
- Added tool declarations (tool15-tool19) to `handle_initialize`
- Added routing in `process_request_sync`

### New Plugin Methods
**src/usd_plugin.h** (lines 103-109, 124-172):
- Added public methods for Phase 1 commands
- Added thread-safe request structures
- Added deferred helper declarations
- Added pending operation maps

**src/usd_plugin.cpp**:
- Lines 65-76: Bound all methods with `ClassDB::bind_method`
- Lines 196-310: Registered callbacks with MCP server
- Lines 1076-1298: Implemented command methods
- Lines 1278-1490: Implemented deferred helpers and bounding box

### Key Implementation Details

**Duplicate Node** (lines 1218-1270):
```cpp
int duplicate_flags = Node::DUPLICATE_SIGNALS |
                     Node::DUPLICATE_GROUPS |
                     Node::DUPLICATE_SCRIPTS |
                     Node::DUPLICATE_USE_INSTANTIATION;  // Critical for deep copy!

Node *duplicated = Object::cast_to<Node>(target_node->duplicate(duplicate_flags));

// Recursively set owner for all children
std::function<void(Node*)> set_owner_recursive = [&](Node* node) {
    node->set_owner(scene_root);
    for (int i = 0; i < node->get_child_count(); i++) {
        set_owner_recursive(node->get_child(i));
    }
};
set_owner_recursive(duplicated);
```

**Bounding Box** (lines 1421-1490):
- Recursive AABB collection from VisualInstance3D nodes
- Global transform application
- Merge all AABBs into combined bounds

---

## Next Steps

### Phase 2: Script Creation & Attachment
From `MCP_COMMANDS_PLAN.md`:

**Priority Commands**:
1. `godot/create_script` - Create GDScript files
2. `godot/attach_script` - Attach scripts to nodes
3. `godot/edit_script` - Modify script content
4. `godot/get_script` - Read script content
5. `godot/create_node` - Create new nodes (Camera3D, lights, etc.)

**Goal**: Add car controller scripts and camera to the racing game scene.

### Suggested First Task
Create a car controller script:
1. Use `godot/create_script` to write `car_controller.gd`
2. Use `godot/attach_script` to attach to formula_car
3. Test basic input handling (WASD for movement)

---

## Build & Deploy

```bash
# Build
cd /Users/nporcino/dev/Lab/usd-godot/build
cmake --build . --parallel

# Deploy
cp /Users/nporcino/dev/Lab/usd-godot/bin/libgodot-usd.dylib \
   /Users/nporcino/dev/Lab/usd-godot/test_project/addons/godot-usd/lib/libgodot-usd.dylib

cp /Users/nporcino/dev/Lab/usd-godot/bin/libgodot-usd.dylib \
   /Users/nporcino/dev/Lab/usd-godot/test_project/lib/libgodot-usd.dylib
```

---

## Testing Commands

### Test Duplication with Deep Copy
```bash
curl -X POST http://127.0.0.1:3000/message -d '{
  "jsonrpc":"2.0","id":"1",
  "method":"godot/duplicate_node",
  "params":{"node_path":"formula_car","new_name":"car4"}
}'
```

### Test Property Update
```bash
curl -X POST http://127.0.0.1:3000/message -d '{
  "jsonrpc":"2.0","id":"2",
  "method":"godot/update_node_property",
  "params":{"node_path":"car4","property":"position","value":"[600.0, 0.0, 0.0]"}
}'
```

### Test Bounding Box
```bash
curl -X POST http://127.0.0.1:3000/message -d '{
  "jsonrpc":"2.0","id":"3",
  "method":"godot/get_bounding_box",
  "params":{"node_path":"car4"}
}'
```

### Test Scene Query
```bash
# Request query
curl -X POST http://127.0.0.1:3000/message -d '{
  "jsonrpc":"2.0","id":"4",
  "method":"godot/query_scene_tree",
  "params":{"path":"/"}
}'

# Poll for result (use ack token from response)
curl -X POST http://127.0.0.1:3000/message -d '{
  "jsonrpc":"2.0","id":"5",
  "method":"godot/dtack",
  "params":{"ack":"ack_XXXXX"}
}'
```

### Test Save
```bash
curl -X POST http://127.0.0.1:3000/message -d '{
  "jsonrpc":"2.0","id":"6",
  "method":"godot/save_scene",
  "params":{}
}'
```

---

## Known Issues & Quirks

### Scale Mismatch
The USD car assets are authored at ~250x scale (possibly cm instead of m):
- Actual car width: 253.78 units
- Actual car length: 561.55 units
- Use `get_bounding_box` to understand real dimensions before positioning

### Geometry Offset
Car geometry has a 5.83-unit offset in X from its pivot point. This is baked into the asset and doesn't need correction when positioning by pivot points.

### Path Format
Always use relative paths without leading slashes for node queries. Scene root is referenced as `"/"` only for `query_scene_tree`.

---

## Reference Files

**Planning**:
- `/Users/nporcino/dev/Lab/usd-godot/MCP_COMMANDS_PLAN.md` - Full command roadmap

**Test Assets**:
- `/Users/nporcino/dev/assets/assets/full_assets/Vehicles/USD_Mini_Car_Kit/assets/vehicles/formula/asset/formulaFullAsset.usda` - Formula car used in testing

**Implementation Reference**:
- Previous MCP implementation: `/Users/nporcino/dev/Lab/GodotSwiftMcpTriadic/addons/godot_mcp`

---

## Session Handoff Checklist

When starting a new session:

1. ✅ MCP server is running in Godot (auto-starts with plugin)
2. ✅ Test scene has 3 cars properly positioned
3. ✅ All Phase 1 commands are functional
4. ✅ Deep copy issue is fixed
5. ✅ `.mcp.json` exists for native MCP integration
6. 📋 Ready to implement Phase 2 (Script Creation)

**Restart Claude Code** to enable native MCP tool integration, or continue using curl for direct testing.

---

## Quick Start Commands

```bash
# Check server is responding
curl -X POST http://127.0.0.1:3000/message -d '{"jsonrpc":"2.0","id":"ping","method":"initialize","params":{}}'

# Query current scene
curl -X POST http://127.0.0.1:3000/message -d '{"jsonrpc":"2.0","id":"1","method":"godot/query_scene_tree","params":{"path":"/"}}'

# Get result (replace ack token)
curl -X POST http://127.0.0.1:3000/message -d '{"jsonrpc":"2.0","id":"2","method":"godot/dtack","params":{"ack":"ack_XXXXX"}}'
```

---

**Last Updated**: 2026-01-15
**Session**: Claude Code continuation from context compaction
**Status**: Phase 1 complete, ready for Phase 2 implementation
