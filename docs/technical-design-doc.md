# Technical Design Document: Godot-USD Integration

**Document Version:** 1.0  
**Date:** April 3, 2025  
**Author:** Systems Architecture Team

## 1. Executive Summary

This document describes the technical implementation of a Godot plugin that integrates Universal Scene Description (USD) into the Godot Engine. The core architectural approach uses USD's Hydra rendering system with Scene Index Filters (SIFs) as an intermediary layer, allowing Godot to function as a render delegate. This integration enables Godot to leverage USD's powerful scene description capabilities while maintaining its real-time interactive features.

## 2. Background and Context

Godot Engine and USD represent two powerful but fundamentally different approaches to scene representation:

- **Godot** uses a scene tree of nodes with class inheritance and real-time signaling
- **USD** uses composition of typed schemas across layers with non-destructive workflows

The integration aims to bridge these systems by leveraging Hydra's Scene Index architecture to provide a clean abstraction layer between USD data representation and Godot scene objects.

## 3. System Design

### 3.1 Architecture Overview

The implementation consists of these primary components:

1. **OpenUSD Integration Plugin**: C++ plugin that embeds USD libraries in Godot
2. **Scene Index Bridge**: Synchronizes between USD's scene representation and Godot's scene tree 
3. **Godot Render Delegate**: Implements Hydra's render delegate interface for Godot
4. **Editor Extensions**: UI components for USD asset handling and editing
5. **Scripting Interface**: GDScript/C# API for USD interaction

![System Architecture Diagram](https://placeholder-image.com/arch_diagram)

### 3.2 Core Integration Approach

The key insight is using **Scene Index Filters (SIFs)** as an abstraction layer:

- USD scene data flows through Hydra's Scene Index
- Godot references scene elements by path rather than direct USD object references
- A dedicated USD thread isolates USD's memory management and threading model
- Communication happens via message passing rather than shared memory

### 3.3 Component Specifications

#### 3.3.1 OpenUSD Integration Plugin

```cpp
class OpenUSDPlugin : public EditorPlugin {
public:
    static void _register_types();
    static void _unregister_types();
    
    virtual void _enter_tree() override;
    virtual void _exit_tree() override;
    
private:
    UsdBridge* m_bridge;
};
```

Key responsibilities:
- Initialize USD libraries and Hydra
- Manage USD stage lifecycle
- Register resource types and import handlers

#### 3.3.2 Scene Index Bridge

```cpp
class UsdBridge {
private:
    // The thread that owns all USD objects
    std::thread m_usdThread;
    
    // Thread-safe command queue (Godot → USD)
    ThreadSafeQueue<UsdCommand> m_commandQueue;
    
    // Flag to control thread lifetime
    std::atomic<bool> m_running;
    
    // The USD stage and scene index
    UsdStageRefPtr m_stage;
    HdSceneIndexBaseRefPtr m_sceneIndex;
    
    // Maps from SIF paths to Godot node IDs
    std::unordered_map<SdfPath, ObjectID, SdfPath::Hash> m_pathToNodeMap;
    
    // Maps from Godot UIDs to SDF paths for stable references
    std::unordered_map<std::string, SdfPath> m_uidToPathMap;
    
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
    
public:
    // Thread-safe API for Godot to send commands
    void QueueCommand(const UsdCommand& cmd) {
        m_commandQueue.enqueue(cmd);
    }
    
    // Start/stop the USD thread
    void Initialize() {
        m_running = true;
        m_usdThread = std::thread(&UsdBridge::ThreadFunction, this);
    }
    
    void Shutdown() {
        m_running = false;
        if (m_usdThread.joinable()) {
            m_usdThread.join();
        }
    }
    
    // Layer management
    void OpenStage(const std::string& path);
    void SaveStage(const std::string& path);
    void ReloadLayer(const SdfLayerHandle& layer);
    
    // Path tracking and property access
    void TrackPath(const SdfPath& path, ObjectID godotNodeId);
    GodotPropertyValue QueryProperty(const SdfPath& path, const TfToken& property);
};
```

Key responsibilities:
- Maintain USD thread and command queue
- Map between USD paths and Godot node IDs
- Handle stage and layer lifecycle
- Process scene changes

#### 3.3.3 Lifecycle Management 

```cpp
struct PathBasedState {
    SdfPath usdPath;
    std::string primType;
    ObjectID godotNodeId;
    // Additional identifying metadata
    std::map<std::string, std::string> metadata;
};

std::vector<PathBasedState> CaptureCurrentState(const SdfLayerHandle& layerToReload) {
    std::vector<PathBasedState> state;
    // Iterate through the scene index for prims affected by this layer
    for (const auto& entry : m_sceneIndex->GetEntries()) {
        if (IsPrimAffectedByLayer(entry.first, layerToReload)) {
            PathBasedState pathState;
            pathState.usdPath = entry.first;
            pathState.primType = entry.second.primType;
            pathState.godotNodeId = m_pathToNodeMap[entry.first];
            // Capture identifying metadata 
            pathState.metadata = GetPrimMetadata(entry.first);
            state.push_back(pathState);
        }
    }
    return state;
}

void ReloadLayer(const SdfLayerHandle& layer) {
    // 1. Capture current state
    auto preReloadState = CaptureCurrentState(layer);
    
    // 2. Perform the reload
    layer->Reload();
    
    // 3. Allow stage to recompose and scene index to update
    m_stage->ProcessAllQueues();
    
    // 4. Analyze changes
    AnalyzeChanges(preReloadState);
}

void AnalyzeChanges(const std::vector<PathBasedState>& preReloadState) {
    // Lists for different change types
    std::vector<SdfPath> deletedPaths;
    std::vector<SdfPath> createdPaths;
    std::vector<std::pair<SdfPath, SdfPath>> updatedPaths; // old path -> new path
    
    // Current state after reload
    std::set<SdfPath> currentPaths;
    for (const auto& entry : m_sceneIndex->GetEntries()) {
        currentPaths.insert(entry.first);
    }
    
    // Find deleted paths (in pre-state but not current)
    for (const auto& oldState : preReloadState) {
        if (currentPaths.find(oldState.usdPath) == currentPaths.end()) {
            // Try to find a matching prim that might have moved
            SdfPath newPath = FindMatchingPrim(oldState);
            if (newPath.IsEmpty()) {
                // Truly deleted
                deletedPaths.push_back(oldState.usdPath);
            } else {
                // Path changed
                updatedPaths.emplace_back(oldState.usdPath, newPath);
            }
        }
    }
    
    // Find created paths (in current but not pre-state)
    std::set<SdfPath> oldPaths;
    for (const auto& state : preReloadState) {
        oldPaths.insert(state.usdPath);
    }
    
    for (const auto& path : currentPaths) {
        if (oldPaths.find(path) == oldPaths.end() && 
            !IsPathInUpdatedList(path, updatedPaths)) {
            createdPaths.push_back(path);
        }
    }
    
    // Notify Godot
    NotifyGodotOfChanges(deletedPaths, createdPaths, updatedPaths);
}

SdfPath FindMatchingPrim(const PathBasedState& oldState) {
    // Search for prims with matching metadata/attributes
    for (const auto& entry : m_sceneIndex->GetEntries()) {
        if (entry.second.primType == oldState.primType) {
            auto currentMetadata = GetPrimMetadata(entry.first);
            
            // Check for matching identifying metadata
            // (could be assetInfo:id, kind, or custom metadata)
            bool isMatch = true;
            for (const auto& idKey : m_identifyingMetadataKeys) {
                if (oldState.metadata.count(idKey) && currentMetadata.count(idKey)) {
                    if (oldState.metadata.at(idKey) != currentMetadata.at(idKey)) {
                        isMatch = false;
                        break;
                    }
                }
            }
            
            if (isMatch) {
                return entry.first;
            }
        }
    }
    
    return SdfPath();
}

void NotifyGodotOfChanges(
    const std::vector<SdfPath>& deletedPaths,
    const std::vector<SdfPath>& createdPaths,
    const std::vector<std::pair<SdfPath, SdfPath>>& updatedPaths) {
    
    // Use Godot's deferred calls to make these notifications thread-safe
    
    // 1. First process deletions
    for (const auto& path : deletedPaths) {
        auto nodeId = m_pathToNodeMap[path];
        Godot::get_singleton()->call_deferred("emit_signal", "usd_prim_deleted", nodeId);
        m_pathToNodeMap.erase(path);
    }
    
    // 2. Then process creations
    for (const auto& path : createdPaths) {
        auto entry = m_sceneIndex->GetPrimEntry(path);
        Godot::get_singleton()->call_deferred("emit_signal", "usd_prim_created", 
            String(path.GetText()), String(entry.primType.GetText()));
    }
    
    // 3. Finally process updates (path changes)
    for (const auto& pathPair : updatedPaths) {
        auto nodeId = m_pathToNodeMap[pathPair.first];
        // Update the mapping
        m_pathToNodeMap.erase(pathPair.first);
        m_pathToNodeMap[pathPair.second] = nodeId;
        
        // Notify Godot
        Godot::get_singleton()->call_deferred("emit_signal", "usd_prim_updated", 
            nodeId, String(pathPair.second.GetText()));
    }
}
```

Key responsibilities:
- Track prim state before layer reloads
- Identify created, deleted, and updated prims
- Maintain object identity across reloads
- Notify Godot of structural changes

#### 3.3.4 Godot Render Delegate

```cpp
class GodotRenderDelegate : public HdRenderDelegate {
public:
    GodotRenderDelegate();
    virtual ~GodotRenderDelegate();
    
    // Required HdRenderDelegate interface
    virtual HdRenderParam* GetRenderParam() const override;
    virtual HdResourceRegistrySharedPtr GetResourceRegistry() const override;
    
    // Resource creation
    virtual HdMesh* CreateMesh(SdfPath const& id) override;
    virtual HdCamera* CreateCamera(SdfPath const& id) override;
    virtual HdLight* CreateLight(SdfPath const& id) override;
    virtual HdMaterial* CreateMaterial(SdfPath const& id) override;
    
    // Resource destruction
    virtual void DestroyMesh(HdMesh* mesh) override;
    virtual void DestroyCamera(HdCamera* camera) override;
    virtual void DestroyLight(HdLight* light) override;
    virtual void DestroyMaterial(HdMaterial* material) override;
    
private:
    std::unique_ptr<GodotRenderParam> m_renderParam;
    HdResourceRegistrySharedPtr m_resourceRegistry;
};
```

Key responsibilities:
- Implement Hydra's render delegate interface
- Create Godot resources for USD scene data
- Map USD render objects to Godot scene objects

#### 3.3.5 Node Creation and Mapping

```cpp
void CreateGodotNodeForUsdPrim(const SdfPath& primPath, const UsdPrim& prim) {
    // Create the Godot node
    Node* node = CreateAppropriateNodeType(prim);
    
    // Generate or use existing UID
    String uid;
    if (prim.HasCustomDataKey(TfToken("godot:uid"))) {
        uid = String(prim.GetCustomDataValue(TfToken("godot:uid")).Get<std::string>().c_str());
    } else {
        uid = GenerateUniqueUID();
        // Store UID back to USD for persistence
        UsdStage::EditPrim(prim).SetCustomData(TfToken("godot:uid"), uid.utf8().get_data());
    }
    
    // Set UID on Godot node
    node->set_meta("uid", uid);
    
    // Add to scene and track in mapping
    AddNodeToScene(node);
    m_pathToNodeMap[primPath] = node->get_instance_id();
    m_uidToNodeMap[uid.utf8().get_data()] = node->get_instance_id();
}

Node* CreateAppropriateNodeType(const UsdPrim& prim) {
    // Map USD schema types to Godot node types
    if (prim.IsA<UsdGeomMesh>()) {
        return memnew(MeshInstance3D);
    } else if (prim.IsA<UsdGeomCamera>()) {
        return memnew(Camera3D);
    } else if (prim.IsA<UsdLuxLight>()) {
        // Determine light type and create appropriate Godot light
        if (UsdLuxDistantLight distantLight = UsdLuxDistantLight(prim)) {
            return memnew(DirectionalLight3D);
        } else if (UsdLuxSphereLight sphereLight = UsdLuxSphereLight(prim)) {
            return memnew(OmniLight3D);
        } else if (UsdLuxDiskLight diskLight = UsdLuxDiskLight(prim)) {
            return memnew(SpotLight3D);
        }
    }
    
    // Default to Node3D for other prim types
    return memnew(Node3D);
}
```

Key responsibilities:
- Create appropriate Godot node types for USD prims
- Establish UID-based identity mapping
- Handle USD schema to Godot node type conversion

#### 3.3.6 UI Extensions

```cpp
class UsdEditorPlugin : public EditorPlugin {
public:
    UsdEditorPlugin(EditorNode* p_node);
    ~UsdEditorPlugin();
    
    virtual String get_name() const override { return "USD"; }
    
    virtual bool has_main_screen() const override { return false; }
    virtual void edit(Object* p_object) override;
    virtual bool handles(Object* p_object) const override;
    virtual void make_visible(bool p_visible) override;
    
private:
    UsdStageEditor* m_stageEditor;
    UsdLayerPanel* m_layerPanel;
    UsdImportDialog* m_importDialog;
};
```

Key responsibilities:
- Provide USD-specific UI extensions for the Godot editor
- Handle USD file importing with appropriate options
- Enable USD layer and variant management

#### 3.3.7 Scripting API

```gdscript
class_name UsdStage
extends Resource

# Open and manage stages
func open(path: String) -> bool
func save(path: String = "") -> bool
func reload() -> void
func get_root_layer_path() -> String

# Layer management
func get_layer_stack() -> Array[UsdLayer]
func set_edit_target(layer: UsdLayer) -> void
func get_edit_target() -> UsdLayer

# Prim access
func get_prim_at_path(path: String) -> UsdPrim
func get_default_prim() -> UsdPrim
func traverse() -> Array[UsdPrim]

# Time management
func set_time_code(time: float) -> void
func get_time_code() -> float
func get_start_time_code() -> float
func get_end_time_code() -> float
```

Key responsibilities:
- Provide GDScript/C# access to USD functionality
- Enable runtime USD manipulation from game code
- Support querying and traversing USD stage data

## 4. Implementation Plan

### 4.1 Core Components

1. **Phase 1: USD Foundation**
   - Integrate USD libraries into Godot build system
   - Implement basic stage loading/saving
   - Create fundamental resource types

2. **Phase 2: Scene Index Bridge**
   - Implement scene index notification system
   - Build path-to-node mapping infrastructure
   - Create threading model and command queue

3. **Phase 3: Render Delegate**
   - Implement Hydra render delegate interface
   - Create mesh conversion systems
   - Handle lighting and cameras

4. **Phase 4: Editor Integration**
   - Develop asset browser extensions
   - Create USD-specific property panels
   - Build layer management UI

5. **Phase 5: Scripting API**
   - Design and implement GDScript/C# bindings
   - Create documentation and examples
   - Build sample projects

### 4.2 Dependency Graph

```
USD Libraries -> Scene Index Bridge -> Render Delegate -> Editor Integration -> Scripting API
     |                  |                   |                   |
     v                  v                   v                   v
Memory Management <- Threading Model <- Resource Conversion <- UI Components
```

### 4.3 Key Challenges and Solutions

| Challenge | Solution Approach |
|-----------|-------------------|
| USD library integration | Use USD's CMake modules to integrate with Godot's SCons system |
| Memory management | Use SIF-based path indirection to avoid direct USD-Godot object references |
| Threading model differences | Dedicated USD thread with command queue pattern |
| Layer reload stability | Path-based state capture with UID-based correlation |
| Schema-to-node mapping | Extensible mapping system with fallbacks to generic nodes |
| Bidirectional editing | Layer-aware property editing with edit target selection |

## 5. Testing Strategy

### 5.1 Unit Testing

- Test USD library initialization and shutdown
- Verify scene index change propagation
- Validate command queue execution
- Ensure proper cleanup during reloads

### 5.2 Integration Testing

- Test importing various USD asset types
- Verify bidirectional property editing
- Validate layer reloading with identity preservation
- Test threading model under load

### 5.3 Performance Testing

- Measure scene load times at various complexities
- Evaluate memory usage patterns
- Assess threading efficiency
- Profile render delegate performance

## 6. Deployment Considerations

### 6.1 Dependencies

- USD libraries (core, hydra, imaging)
- TBB (Threading Building Blocks)
- Boost (used by USD)
- Python (for USD script plugins, optional)

### 6.2 Platform Support

- Initially target Godot 4.x on desktop platforms (Windows, macOS, Linux)
- Mobile and web support would require additional optimization

### 6.3 Distribution

- Package as a Godot plugin (.zip or .gdextension)
- Provide precompiled binaries for common platforms
- Document build process for source compilation

## 7. Future Extensions

- **USD Physics Integration**: Connect USD physics schemas to Godot's physics engine
- **Procedural Asset Generation**: Use USD's procedural generation capabilities
- **Collaborative Editing**: Leverage USD's layer-based workflow for multi-user editing
- **Animation Retargeting**: Utilize USD's animation systems for advanced retargeting
- **Material Conversion**: Advanced PBR material translation between USD and Godot

## 8. Plugin Usage Examples

### 8.1 Importing USD Assets

```gdscript
func import_usd_asset():
    var usd_importer = UsdImporter.new()
    var import_opts = UsdImportOptions.new()
    
    # Configure import options
    import_opts.import_materials = true
    import_opts.import_animations = true
    import_opts.flatten_stage = false
    
    # Import the USD file
    var result = usd_importer.import("res://assets/character.usd", import_opts)
    if result:
        print("USD asset imported successfully")
    else:
        printerr("Failed to import USD asset")
```

### 8.2 Switching USD Variants

```gdscript
func switch_variant(stage_resource, variant_set, variant_name):
    # Get the stage from the resource
    var stage = stage_resource.get_stage()
    
    # Get the prim with variants
    var root_prim = stage.get_default_prim()
    
    # Set the variant selection
    if root_prim.has_variant_set(variant_set):
        root_prim.set_variant_selection(variant_set, variant_name)
        
        # Request a scene update to reflect the changes
        get_tree().call_group("usd_listeners", "on_variant_changed")
```

### 8.3 Programmatically Creating USD Content

```gdscript
func create_usd_content():
    # Create a new USD stage
    var stage = UsdStage.new()
    stage.create_new("res://output.usda")
    
    # Create a default prim to serve as the root
    var root_prim = stage.define_prim("/World", "Xform")
    stage.set_default_prim(root_prim)
    
    # Add a sphere
    var sphere = stage.define_prim("/World/Sphere", "Sphere")
    sphere.create_attribute("radius", UsdTypes.FLOAT, 2.0)
    
    # Add a cube
    var cube = stage.define_prim("/World/Cube", "Cube")
    cube.create_attribute("size", UsdTypes.FLOAT, 1.0)
    
    # Position the cube
    var xform_api = UsdGeomXformAPI.new(cube)
    xform_api.set_translate(Vector3(3.0, 0.0, 0.0))
    
    # Save the stage
    stage.save()
```

## 9. Conclusion

This technical design document outlines a comprehensive approach to integrating USD into Godot using Hydra's Scene Index Filter architecture. The design leverages the strengths of both systems while maintaining clean boundaries between them. By using path-based indirection and a dedicated threading model, we avoid complex cross-system memory management issues and create a robust foundation for future development.

The implementation described here will enable Godot developers to work with USD assets directly within Godot, combining USD's powerful scene description capabilities with Godot's interactive runtime features. This integration opens new possibilities for collaborative workflows, high-end visualization, and cross-platform deployment of complex 3D content.

## 10. References

- [USD Documentation](https://openusd.org/release/index.html)
- [Hydra Architecture](https://openusd.org/release/api/hd_page_front.html)
- [Godot Engine Documentation](https://docs.godotengine.org/)
- [Pixar's USD GitHub Repository](https://github.com/PixarAnimationStudios/USD)
- [Godot 4.0 GDExtension Documentation](https://docs.godotengine.org/en/latest/tutorials/scripting/gdextension/what_is_gdextension.html)