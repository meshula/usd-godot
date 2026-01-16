# MCP Commands Implementation Plan

## Overview
This document tracks the implementation of MCP commands needed to build a racing game with the USD-Godot plugin. Commands are organized by phase, with Phase 1 focusing on immediate needs for positioning race cars.

## Reference Implementation
Previous MCP implementation: `/Users/nporcino/dev/Lab/GodotSwiftMcpTriadic/addons/godot_mcp`

---

## Phase 1: Essential Scene Manipulation (Racing Game Startup)
**Goal:** Add 3 cars to scene and position them on starting line

### Scene Query Commands
- [x] `godot/query_scene_tree` - Query scene hierarchy (non-recursive, path-based)
  - **Status:** ✅ COMPLETE - Implemented with ACK/DTACK pattern
  - Returns: node name, type, path, child_count, groups, transform
  - Usage: `{"path": "/"}` for root, `{"path": "node_name"}` for children

- [x] `godot/dtack` - Poll async operation status
  - **Status:** ✅ COMPLETE - ACK/DTACK polling infrastructure working
  - Returns: status (pending/complete/error/canceled), message, data

### Node Property Commands
- [ ] `godot/get_node_properties` - Get all properties of a node
  - **Priority:** HIGH - Need to inspect node properties
  - Params: `node_path` (string, relative to scene root)
  - Returns: Dictionary of property names → values
  - Use case: Inspect car node to see available properties

- [ ] `godot/update_node_property` - Update a single property on a node
  - **Priority:** CRITICAL - Required to position cars
  - Params: `node_path`, `property`, `value`
  - Returns: Success confirmation with updated value
  - Use case: Set `position`, `rotation` on car nodes
  - **REQUIRED FOR:** Positioning cars on starting line

### Node Manipulation Commands
- [ ] `godot/duplicate_node` - Duplicate an existing node
  - **Priority:** CRITICAL - Need to create 3 cars from 1
  - Params: `node_path`, `new_name` (optional)
  - Returns: New node path
  - Use case: Duplicate formula_car to create car2, car3
  - **REQUIRED FOR:** Creating multiple cars from single USD import

- [ ] `godot/rename_node` - Rename a node
  - **Priority:** MEDIUM - Nice to have for organization
  - Params: `node_path`, `new_name`
  - Returns: New node path
  - Use case: Rename duplicated cars to car1, car2, car3

### Scene Management Commands
- [ ] `godot/save_scene` - Save the current scene
  - **Priority:** HIGH - Persist changes after positioning cars
  - Params: `path` (optional, uses current scene path if empty)
  - Returns: Scene path
  - Use case: Save scene after positioning cars

---

## Phase 2: Script Creation & Attachment
**Goal:** Add scripts to control cars and camera

### Script Commands
- [ ] `godot/create_script` - Create a new GDScript file
  - Priority: HIGH
  - Params: `script_path`, `content`, `node_path` (optional, for attaching)
  - Returns: Script path, attached node path
  - Use case: Create car_controller.gd for race car logic

- [ ] `godot/edit_script` - Edit existing script content
  - Priority: MEDIUM
  - Params: `script_path`, `content`
  - Returns: Script path
  - Use case: Modify car controller logic

- [ ] `godot/get_script` - Read script file contents
  - Priority: MEDIUM
  - Params: `script_path`
  - Returns: Script content as string
  - Use case: Read script to understand current implementation

- [ ] `godot/attach_script` - Attach a script to a node
  - Priority: HIGH
  - Params: `node_path`, `script_path`
  - Returns: Success confirmation
  - Use case: Attach car_controller.gd to formula_car node

### Node Creation Commands
- [ ] `godot/create_node` - Create a new node
  - Priority: MEDIUM
  - Params: `parent_path`, `node_type`, `node_name`
  - Returns: Node path
  - Use case: Add Camera3D, DirectionalLight3D, RigidBody3D for physics

- [ ] `godot/delete_node` - Delete a node
  - Priority: LOW
  - Params: `node_path`
  - Returns: Deleted node path
  - Use case: Remove test nodes

---

## Phase 3: Scene & Resource Management
**Goal:** Create race tracks, manage resources

### Scene Commands
- [ ] `godot/create_scene` - Create a new scene file
  - Priority: MEDIUM
  - Params: `path`, `root_node_type`
  - Returns: Scene path
  - Use case: Create track.tscn, ui.tscn

- [ ] `godot/open_scene` - Open a scene in the editor
  - Priority: MEDIUM
  - Params: `path`
  - Returns: Scene path
  - Use case: Switch between scenes

- [ ] `godot/get_current_scene` - Get current scene info
  - Priority: MEDIUM
  - Params: None
  - Returns: scene_path, root_node_type, root_node_name
  - Use case: Verify which scene is being edited

- [ ] `godot/get_scene_structure` - Get full scene hierarchy
  - Priority: LOW (have query_scene_tree for this)
  - Params: `path`
  - Returns: Full recursive node structure
  - Use case: Analyze scene files

### Resource Commands
- [ ] `godot/list_project_resources` - List resources by type
  - Priority: LOW
  - Params: `resource_type` (optional)
  - Returns: Array of resource paths
  - Use case: Find USD files, materials, textures

---

## Phase 4: Physics & Game Logic
**Goal:** Add physics, collisions, game rules

### Physics Property Commands
- [ ] `godot/set_physics_properties` - Batch set physics properties
  - Priority: HIGH (when adding physics)
  - Params: `node_path`, `properties` (dict)
  - Returns: Updated properties
  - Use case: Set mass, friction, collision layers on car

### Input & Configuration
- [ ] `godot/get_input_map` - Get input action mappings
  - Priority: MEDIUM
  - Params: None
  - Returns: Input action map
  - Use case: Understand current controls

- [ ] `godot/set_input_action` - Set input action mapping
  - Priority: MEDIUM
  - Params: `action_name`, `keys/buttons`
  - Returns: Success confirmation
  - Use case: Configure car controls (accelerate, brake, steer)

---

## Phase 5: UI & HUD
**Goal:** Add racing UI, lap timer, speedometer

### UI Node Commands
- [ ] `godot/create_ui_node` - Create UI control nodes
  - Priority: MEDIUM (when building UI)
  - Params: `parent_path`, `control_type`, `node_name`
  - Returns: Node path
  - Use case: Create Label, ProgressBar, Panel for HUD

- [ ] `godot/set_ui_properties` - Set UI-specific properties
  - Priority: MEDIUM
  - Params: `node_path`, `properties`
  - Returns: Updated properties
  - Use case: Set text, anchors, margins

---

## Phase 6: Advanced Features
**Goal:** Track management, AI opponents, multiplayer prep

### Advanced Scene Commands
- [ ] `godot/instantiate_scene` - Instance a scene as child
  - Priority: MEDIUM
  - Params: `parent_path`, `scene_path`, `node_name`
  - Returns: Instance root node path
  - Use case: Spawn opponent cars, checkpoints

- [ ] `godot/get_node_children_recursive` - Get all descendants
  - Priority: LOW
  - Params: `node_path`, `max_depth` (optional)
  - Returns: Array of child node info
  - Use case: Find all mesh instances for LOD management

### Signal & Connection Commands
- [ ] `godot/connect_signal` - Connect a signal to a method
  - Priority: LOW (script-based connections easier)
  - Params: `source_node`, `signal_name`, `target_node`, `method_name`
  - Returns: Success confirmation
  - Use case: Connect collision signals for checkpoints

---

## Implementation Notes

### Phase 1 Priorities (IMMEDIATE)
For the goal of "add 3 cars to scene and position them on starting line":
1. **CRITICAL:** `godot/duplicate_node` - Create car2 and car3
2. **CRITICAL:** `godot/update_node_property` - Position each car
3. **HIGH:** `godot/get_node_properties` - Inspect available properties
4. **HIGH:** `godot/save_scene` - Persist the changes

### Command Implementation Pattern
All commands follow the JSON-RPC 2.0 pattern:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "godot/command_name",
  "params": {
    "param1": "value1",
    "param2": "value2"
  }
}
```

### Thread Safety
All commands that modify the scene tree must:
1. Use `call_deferred` to schedule on main thread
2. Return immediately (or use ACK/DTACK if waiting needed)
3. Be bound with `ClassDB::bind_method` for reflection

### Error Handling
Commands return JSON-RPC error responses:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "error": {
    "code": -32603,
    "message": "Error description"
  }
}
```

### Success Responses
Commands return JSON-RPC success responses:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": {
    "success": true,
    "data": {...}
  }
}
```

---

## Current Status
**Completed:**
- USD stage management (create, list, reflect)
- Scene tree queries (ACK/DTACK pattern)
- Async operation infrastructure

**Next Up (Phase 1):**
- Node property commands (get/update)
- Node duplication command
- Scene save command

**Total Progress:** 2/40+ commands implemented (5%)
