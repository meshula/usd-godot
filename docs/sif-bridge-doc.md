# Scene Index Filter Bridge: System Design

## Introduction

This document outlines the design for a USD-Godot integration based on Hydra's Scene Index Filter (SIF) architecture. Rather than attempting to directly map USD objects to Godot nodes, this approach uses Hydra's scene indexing system as an abstraction layer, providing cleaner system boundaries and simplified memory management.

## Background: Hydra and Scene Indices

Hydra is USD's rendering architecture that decouples scene representation from rendering implementation. A key component of Hydra is the Scene Index, which provides a queryable representation of the USD scene that has been processed for rendering.

Scene Index Filters (SIFs) allow for runtime transformation of the scene representation before it reaches the render delegate. This creates a separation between the "authored USD scene" and the "evaluated scene for rendering" - a separation we'll leverage for our Godot integration.

## Architectural Overview

The proposed architecture uses Hydra's Scene Index as the bridge between USD and Godot:

![System Architecture Diagram](https://placeholder-image.com/arch_diagram)

### Key Components

1. **USD Subsystem**: Contains the USD stage, composition engine, and Hydra infrastructure
2. **Scene Index Bridge**: Translates between USD's scene representation and Godot's scene tree
3. **Godot Render Delegate**: Consumes the processed scene index to drive Godot's rendering

### Data Flow

The system operates with the following data flow:

1. **USD → Godot**: Changes in the USD stage flow through Hydra's Scene Index to Godot
2. **Godot → USD**: Godot editor operations are translated to USD layer edits

## Bridge Design

### Core Principle: Path-Based Indirection

Instead of directly mapping USD objects to Godot nodes, we use path-based indirection:

- Godot nodes store SIF paths/indices rather than USD object references
- When Godot needs USD data, it queries through the SIF rather than holding USD objects
- Changes flow across system boundaries as serializable data, not object references

### Bridge Component

```cpp
class UsdSceneIndexBridge {
private:
    // The Hydra scene index
    HdSceneIndexBaseRefPtr m_sceneIndex;
    
    // Maps from SIF paths to Godot node IDs
    std::unordered_map<SdfPath, ObjectID, SdfPath::Hash> m_pathToNodeMap;
    
    // Maps from Godot UIDs to SDF paths for stable references
    std::unordered_map<std::string, SdfPath> m_uidToPathMap;
    
public:
    // Godot calls this to register interest in a path
    void TrackPath(const SdfPath& path, ObjectID godotNodeId);
    
    // Called by Hydra when the scene index changes
    void OnSceneIndexChanged(const HdSceneIndexBase::DirtiedPrimEntries& entries);
    
    // Godot calls this to query data without holding USD references
    GodotPropertyValue QueryProperty(const SdfPath& path, const TfToken& property);
};
```

## Threading Model

### Dedicated USD Thread

To isolate USD's threading model from Godot's:

1. Run all USD operations on a dedicated thread
2. This thread owns all USD objects and is the only one that directly manipulates them
3. Communication happens via thread-safe message queues

```cpp
class UsdBridge {
private:
    // The thread that owns all USD objects
    std::thread m_usdThread;
    
    // Thread-safe command queue (Godot → USD)
    ThreadSafeQueue<UsdCommand> m_commandQueue;
    
    // The USD stage and scene index
    UsdStageRefPtr m_stage;
    HdSceneIndexBaseRefPtr m_sceneIndex;
    
    void ThreadFunction() {
        while (m_running) {
            // Process any pending commands
            UsdCommand cmd;
            while (m_commandQueue.try_dequeue(cmd)) {
                ProcessCommand(cmd);
            }
            
            // Check for USD changes
            ProcessSceneIndexChanges();
            
            // Prevent busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};
```

### Command Queue Pattern

For Godot → USD communication:
- Commands are pushed to a thread-safe queue
- USD thread processes commands sequentially, ensuring single-writer semantics
- Results/errors returned via callback events

### Notification System

For USD → Godot communication:
- Changes from USD generate notifications
- These are posted to Godot's main thread via its built-in thread-safe call mechanism

## Lifecycle Management

One of the more complex challenges is maintaining identity during USD stage reloads or layer updates.

### Path-Based Identity Mapping

When a USD layer is reloaded:

1. **Pre-Reload State Capture**: 
   - Capture paths, types, and metadata for all affected prims

2. **Layer Reload**:
   - Reload the layer and allow stage recomposition

3. **Change Analysis**:
   - Compare pre-reload state with post-reload state
   - Categorize changes as "deleted", "created", or "updated" (path changes)

4. **Identity Resolution**:
   - For "deleted" prims, attempt to find matching prims based on metadata
   - If matches found, consider them "updated" rather than "deleted" + "created"

5. **Notification**:
   - Notify Godot of all changes, allowing it to update its scene tree

### UID-Based Stability

To enhance stability across reloads:

- Store Godot UIDs in USD prim metadata
- During reloads, prioritize UID matching over path matching
- Maintain UID maps separately from path maps

## Handling Bi-directional Synchronization

### USD → Godot Synchronization

For property changes:
1. The SIF notifies the bridge of changed properties
2. The bridge identifies affected Godot nodes by path
3. Property values are translated to Godot types
4. Godot nodes are updated via deferred calls

For structural changes:
1. The bridge detects prim additions/removals
2. Appropriate Godot nodes are created/removed
3. The path mapping tables are updated

### Godot → USD Synchronization

For property edits:
1. Godot property changes are captured via the inspector
2. The bridge converts values to USD types
3. Edits are applied to the appropriate USD layer via commands

For structural changes:
1. Node additions/removals in Godot generate commands
2. Commands specify which USD layer should receive the changes
3. USD prims are created/removed in the specified layer

## Advantages of the SIF Approach

### Clean Memory Management

- USD memory remains entirely managed by USD's reference counting
- Godot memory remains entirely managed by Godot's system
- No cross-system reference counting to reconcile

### Simplified Threading Model

- USD operations run on a dedicated thread with clear ownership
- Communication via message passing rather than shared memory
- Avoids complex synchronization between different threading models

### Stable References

- Path-based indirection with UID support provides stability
- Changes in USD hierarchy don't break Godot references
- Reload operations can maintain object identity

### Performance Isolation

- Each subsystem can optimize internally
- Batch operations across subsystem boundaries
- Clear boundaries for profiling and optimization

## Conclusion

The Scene Index Filter bridge design provides a robust foundation for integrating USD and Godot. By using Hydra's scene indexing system as an abstraction layer, we achieve clean system boundaries, simplified memory management, and a straightforward threading model. This approach leverages the strengths of both systems while minimizing the complexity of their integration.
