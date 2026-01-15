# USD Stage Manager Implementation

## ✅ Completed Implementation

Successfully implemented a shared USD stage management system with generation tracking, MCP commands, and full testing.

### Date Completed
January 15, 2026

### Status
**✅ WORKING** - Successfully tested with snowman creation

---

## Architecture Overview

### Core Design Principles

1. **Shared Core Utility** - Stage management is NOT owned by MCP or GD bindings, but is a shared system both can use
2. **Generation Tracking** - Every mutation increments a generation counter to track if saves are needed
3. **Private Members with Accessor Functions** - Generation is automatically incremented on all mutations
4. **Thread-Safe** - Uses mutex for concurrent access from MCP and GDScript

### Components

```
┌─────────────────────────────────────┐
│   UsdStageManager (Singleton)       │
│   - Thread-safe stage registry      │
│   - Stage ID allocation             │
│   - Core stage operations           │
└────────────┬────────────────────────┘
             │
             ├──────────────┬──────────────┐
             │              │              │
      ┌──────▼─────┐  ┌────▼────┐   ┌────▼────┐
      │ MCP Server │  │ GDScript│   │  Future │
      │  Commands  │  │ Bindings│   │  Clients│
      └────────────┘  └─────────┘   └─────────┘
```

---

## Implementation Details

### 1. Stage Manager Core (`usd_stage_manager.h/cpp`)

**StageRecord Class:**
- Wraps `UsdStageRefPtr` with generation tracking
- Private members force mutation through accessor functions
- Generation auto-increments on `create_prim()`, `define_prim()`, `mark_modified()`
- Save operations do NOT increment generation (saving doesn't modify content)

**UsdStageManager Class:**
- Thread-safe singleton pattern
- Manages map of `StageId` → `StageRecord`
- Provides stage creation, opening, closing, saving
- Convenience methods for prim creation
- Generation querying

**Key Methods:**
```cpp
StageId create_stage(const std::string& file_path = "");
StageId open_stage(const std::string& file_path);
bool close_stage(StageId id);
bool save_stage(StageId id, const std::string& file_path = "");
uint64_t get_generation(StageId id);
bool create_prim(StageId id, const std::string& path, const std::string& type_name);
```

### 2. MCP Commands

**Added Commands:**

| Command | Purpose | Parameters | Returns |
|---------|---------|------------|---------|
| `usd/create_stage` | Create new stage | `file_path` (optional) | `stage_id`, `generation` |
| `usd/save_stage` | Save stage to file | `stage_id`, `file_path` (optional) | `success`, `stage_id` |
| `usd/query_generation` | Get generation number | `stage_id` | `stage_id`, `generation` |
| `usd/create_prim` | Create prim in stage | `stage_id`, `prim_path`, `prim_type` | `success`, `stage_id`, `prim_path`, `generation` |

**JSON-RPC Examples:**

**Create Stage:**
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "usd/create_stage",
  "params": {
    "file_path": "snowman.usda"
  }
}
```

Response:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": {
    "stage_id": 1,
    "generation": 0
  }
}
```

**Create Prim:**
```json
{
  "jsonrpc": "2.0",
  "id": "2",
  "method": "usd/create_prim",
  "params": {
    "stage_id": 1,
    "prim_path": "/Snowman/Bottom",
    "prim_type": "Sphere"
  }
}
```

Response:
```json
{
  "jsonrpc": "2.0",
  "id": "2",
  "result": {
    "success": true,
    "stage_id": 1,
    "prim_path": "/Snowman/Bottom",
    "generation": 1
  }
}
```

### 3. Generation Tracking

**How It Works:**

1. **Initial Creation**: Generation starts at 0
2. **Each Mutation**: Generation increments (+1)
3. **Queries**: Can check generation to decide if save is needed
4. **Saves**: Do NOT increment generation

**Example Flow:**
```
create_stage()        → generation = 0
create_prim("/A")     → generation = 1
create_prim("/B")     → generation = 2
query_generation()    → returns 2
save_stage()          → generation stays 2 (save doesn't modify)
create_prim("/C")     → generation = 3
```

**Use Cases:**
- Auto-save detection: Compare generation to last saved value
- Dirty flag: Non-zero generation since last save means unsaved changes
- Undo/redo: Track generation for each operation

---

## Testing

### Snowman Test (`test_snowman.sh`)

Successfully creates a USD snowman with 3 sphere prims:

**Test Flow:**
1. Create stage with file path "snowman.usda"
2. Create prim "/Snowman/Bottom" (Sphere) → gen 1
3. Create prim "/Snowman/Middle" (Sphere) → gen 2
4. Create prim "/Snowman/Head" (Sphere) → gen 3
5. Query generation → returns 3
6. Save stage

**Output (snowman.usda):**
```usda
#usda 1.0

def "Snowman"
{
    def Sphere "Bottom"
    {
    }

    def Sphere "Middle"
    {
    }

    def Sphere "Head"
    {
    }
}
```

### Verification

✅ Stage creation works
✅ Prim creation works
✅ Generation tracking increments correctly (0→1→2→3)
✅ Stage saving works
✅ USD file format is correct
✅ All prims appear in hierarchy

---

## Files Modified/Created

```
✨ NEW:
  src/usd_stage_manager.h (90 lines)
  src/usd_stage_manager.cpp (245 lines)
  test_snowman.sh (executable test script)
  STAGE_MANAGER_IMPLEMENTATION.md (this file)

✏️ MODIFIED:
  src/mcp_server.h (added 4 command handlers)
  src/mcp_server.cpp (added ~210 lines of implementation)
  CMakeLists.txt (added stage manager files)
```

---

## Future Work

### Remaining TODO

As per the original plan, we still need to:

1. **Port UsdStageProxy to Use Shared Manager**
   - Currently UsdStageProxy manages its own stage
   - Should use UsdStageManager for consistency
   - This ensures GDScript and MCP see the same stages

2. **Add More USD Operations**
   - Set prim attributes
   - Create relationships
   - Set transforms/positions (to properly position snowman spheres)
   - Query prim properties

3. **MCP Tool Capabilities**
   - Update initialize response with tool capabilities
   - Add proper MCP tool schema definitions

### Note for Future Implementation

**IMPORTANT**: When adding commands that mutate stages, remember to:
- Call methods through `StageRecord` accessors (not direct USD calls)
- Use `mark_modified()` if doing raw USD operations
- The `create_prim()` and `define_prim()` methods already handle this

**Good:**
```cpp
StageRecord* record = UsdStageManager::get_singleton().get_stage_record(id);
record->create_prim(path, type);  // Generation auto-increments
```

**Bad:**
```cpp
StageRecord* record = UsdStageManager::get_singleton().get_stage_record(id);
UsdStageRefPtr stage = record->get_stage();
stage->DefinePrim(path, type);  // Generation NOT incremented!
```

---

## Design Benefits

### Why This Architecture?

1. **Single Source of Truth**: All stage access goes through one manager
2. **Automatic Tracking**: Generation can't be forgotten - it's built into accessors
3. **Thread-Safe**: Multiple clients (MCP, GDScript, future tools) can safely access
4. **Extensible**: Easy to add new operations that automatically track generation
5. **Testable**: Stage manager can be tested independently of Godot/MCP

### Generation Counter Benefits

1. **Dirty Tracking**: Know when saves are needed without manual flags
2. **Change Detection**: Detect if stage changed between operations
3. **Undo/Redo Support**: Track operation count for undo stack depth
4. **Auto-Save Logic**: Save only when generation changed since last save
5. **Multi-User Sync**: Detect conflicts in collaborative editing

---

## Running The Test

```bash
cd /Users/nporcino/dev/Lab/usd-godot
./test_snowman.sh
```

Expected output: Creates `test_project/snowman.usda` with 3 sphere prims.

---

## References

- USD Stage Management: https://openusd.org/dev/api/class_usd_stage.html
- USD Prim Definition: https://openusd.org/dev/api/class_usd_prim.html
- Generation Counter Pattern: Based on optimistic concurrency control
- MCP JSON-RPC: https://modelcontextprotocol.io/specification
