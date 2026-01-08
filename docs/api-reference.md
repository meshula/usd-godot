# GDScript API Reference

This document provides a complete reference for the Godot-USD GDScript API.

## Overview

The plugin provides three main classes:

| Class | Description |
|-------|-------------|
| `UsdStageProxy` | Main interface for working with USD stages (files) |
| `UsdPrimProxy` | Wrapper for USD prims with automatic type conversion |
| `UsdDocument` | High-level import/export between USD and Godot scenes |

All classes extend `RefCounted` and are automatically memory-managed - never call `.free()` on them.

---

## UsdStageProxy

The primary class for opening, creating, and manipulating USD stages.

### Stage Lifecycle

```gdscript
var stage = UsdStageProxy.new()

# Open an existing USD file
var err = stage.open("res://assets/scene.usda")

# Create a new USD file
var err = stage.create_new("res://output/new_scene.usda")

# Save changes
stage.save()                    # Save to original location
stage.save("res://output.usda") # Save to new location

# Export to different format
stage.export_to("res://output.usdc", true)  # binary=true

# Close and release resources
stage.close()

# Reload from disk (discards unsaved changes)
stage.reload()

# Check state
stage.is_open()      # Returns bool
stage.is_modified()  # Returns bool
```

### Prim Access

```gdscript
# Get the default (root) prim
var root = stage.get_default_prim()

# Set the default prim
stage.set_default_prim("/World")

# Get prim at a specific path
var prim = stage.get_prim_at_path("/World/MyMesh")

# Check if prim exists
if stage.has_prim_at_path("/World/MyMesh"):
    print("Prim exists!")

# Traverse all prims (returns Array of UsdPrimProxy)
for prim in stage.traverse():
    print(prim.get_path(), " -> ", prim.get_type_name())

# Traverse by type
for mesh in stage.traverse_by_type("Mesh"):
    print("Found mesh: ", mesh.get_name())
```

### Prim Creation

```gdscript
# Define a new prim (creates parent Xforms automatically)
var xform = stage.define_prim("/Root", "Xform")
var cube = stage.define_prim("/Root/MyCube", "Cube")
var sphere = stage.define_prim("/Root/MySphere", "Sphere")
var camera = stage.define_prim("/Root/Camera", "Camera")

# Remove a prim
stage.remove_prim("/Root/MyCube")
```

### Time / Animation

```gdscript
# Set current evaluation time
stage.set_time_code(24.0)
var current = stage.get_time_code()

# Get time range
var start = stage.get_start_time_code()
var end = stage.get_end_time_code()

# Set time range
stage.set_time_range(1.0, 100.0)

# Frames per second
var fps = stage.get_frames_per_second()
stage.set_frames_per_second(30.0)
```

### Stage Metadata

```gdscript
# Up axis ("Y" or "Z")
var axis = stage.get_up_axis()
stage.set_up_axis("Y")

# Meters per unit scale
var scale = stage.get_meters_per_unit()
stage.set_meters_per_unit(0.01)  # centimeters

# Root layer file path
var path = stage.get_root_layer_path()
```

### Layer Management

```gdscript
# Get sublayer paths
var sublayers = stage.get_sublayer_paths()

# Add/remove sublayers
stage.add_sublayer("./materials.usda")
stage.remove_sublayer("./materials.usda")
```

---

## UsdPrimProxy

Returned by `UsdStageProxy` methods. Provides access to prim properties, attributes, transforms, and more.

### Prim Identity

```gdscript
var prim = stage.get_prim_at_path("/World/MyMesh")

prim.get_name()       # "MyMesh"
prim.get_path()       # "/World/MyMesh"
prim.get_type_name()  # "Mesh", "Xform", "Cube", etc.
prim.is_valid()       # true if prim reference is valid
prim.is_active()      # true if prim is active
prim.set_active(false)  # deactivate prim
```

### Hierarchy Navigation

```gdscript
# Parent
var parent = prim.get_parent()

# Children
var children = prim.get_children()  # Array of UsdPrimProxy
for child in children:
    print(child.get_name())

# Check for child
if prim.has_child("SubMesh"):
    var sub = prim.get_child("SubMesh")

# All descendants (recursive)
var all = prim.get_descendants()
```

### Attributes

USD attributes are automatically converted to/from Godot types:

| USD Type | Godot Type |
|----------|------------|
| float, double | float |
| int | int |
| bool | bool |
| string, token | String |
| GfVec3f/d | Vector3 |
| GfVec4f/d | Color |
| GfMatrix4d | Transform3D |
| VtArray<float> | PackedFloat32Array |
| VtArray<int> | PackedInt32Array |
| VtArray<Vec3f> | PackedVector3Array |

```gdscript
# List attributes
var names = prim.get_attribute_names()

# Check attribute existence
if prim.has_attribute("size"):
    var size = prim.get_attribute("size")

# Get/set attributes
var size = prim.get_attribute("size")
prim.set_attribute("size", 2.5)

# Get at specific time (for animated values)
var value = prim.get_attribute_at_time("translate", 24.0)

# Set at specific time (keyframe)
prim.set_attribute_at_time("translate", Vector3(1, 2, 3), 24.0)

# Remove attribute
prim.remove_attribute("custom_attr")

# Get attribute metadata
var meta = prim.get_attribute_metadata("size")
# Returns Dictionary with type, variability, etc.
```

### Transform Operations

Convenience methods for common transform operations:

```gdscript
# Local transform (relative to parent)
var local = prim.get_local_transform()  # Transform3D
prim.set_local_transform(Transform3D())

# At specific time
var xform = prim.get_local_transform_at_time(24.0)
prim.set_local_transform_at_time(xform, 48.0)

# World transform (accumulated from root)
var world = prim.get_world_transform()
var world_at_time = prim.get_world_transform_at_time(24.0)
```

### Variants

USD variants allow switching between different configurations:

```gdscript
# List variant sets
var variant_sets = prim.get_variant_sets()  # ["LOD", "Material"]

# Check for variant set
if prim.has_variant_set("LOD"):
    # List variants in set
    var variants = prim.get_variants("LOD")  # ["high", "medium", "low"]

    # Get current selection
    var current = prim.get_variant_selection("LOD")

    # Change selection
    prim.set_variant_selection("LOD", "low")
```

### References and Payloads

```gdscript
# References (always loaded)
if prim.has_references():
    print("Prim has references")
prim.add_reference("./other_asset.usda", "/DefaultPrim")

# Payloads (lazy-loaded)
if prim.has_payloads():
    print("Prim has payloads")
prim.add_payload("./heavy_asset.usda")
prim.load_payloads()
prim.unload_payloads()
```

### Relationships

```gdscript
# List relationships
var rels = prim.get_relationship_names()

# Get targets
if prim.has_relationship("material:binding"):
    var targets = prim.get_relationship_targets("material:binding")

# Set targets
prim.set_relationship_targets("material:binding", ["/Materials/Gold"])
prim.add_relationship_target("proxyPrim", "/Proxies/LowRes")
```

### Metadata

```gdscript
# Custom data (arbitrary user data)
var data = prim.get_custom_data()
prim.set_custom_data({"myKey": "myValue", "number": 42})

# Asset info
var info = prim.get_asset_info()
prim.set_asset_info({"name": "MyAsset", "version": "1.0"})

# Documentation
var doc = prim.get_documentation()
prim.set_documentation("This is my documented prim")
```

### Type Checks

```gdscript
prim.is_xform()   # true for Xform prims
prim.is_mesh()    # true for Mesh prims
prim.is_camera()  # true for Camera prims
prim.is_light()   # true for any light type
prim.is_gprim()   # true for geometric primitives (Cube, Sphere, etc.)
```

---

## UsdDocument

High-level class for importing USD files into Godot scenes and exporting Godot scenes to USD.

### Import

```gdscript
var doc = UsdDocument.new()
var state = UsdState.new()
var parent = Node3D.new()
add_child(parent)

var err = doc.import_from_file("res://assets/model.usda", parent, state)
if err == OK:
    print("Import successful!")
    # parent now contains the imported scene hierarchy
```

### Export

```gdscript
var doc = UsdDocument.new()
var state = UsdState.new()

# Build up scene in the document
var err = doc.append_from_scene(my_scene_root, state)
if err == OK:
    # Write to file
    doc.write_to_filesystem(state, "res://output/exported.usda")
```

### File Extensions

```gdscript
var doc = UsdDocument.new()
print(doc.get_file_extension_for_format(false))  # "usda" (ASCII)
print(doc.get_file_extension_for_format(true))   # "usdc" (binary)
```

---

## UsdState

Configuration state for import/export operations.

```gdscript
var state = UsdState.new()
# Currently used as a placeholder for future configuration options
```

---

## Complete Example

```gdscript
extends Node3D

func _ready():
    # Open a USD file
    var stage = UsdStageProxy.new()
    if stage.open("res://assets/scene.usda") != OK:
        print("Failed to open USD file")
        return

    # Print stage info
    print("Up axis: ", stage.get_up_axis())
    print("Meters per unit: ", stage.get_meters_per_unit())

    # Get the default prim
    var root = stage.get_default_prim()
    if root:
        print("Default prim: ", root.get_name())

        # Traverse children
        for child in root.get_children():
            print("  - ", child.get_name(), " (", child.get_type_name(), ")")

            # Check for variants
            if child.has_variant_set("LOD"):
                print("    LOD variants: ", child.get_variants("LOD"))
                child.set_variant_selection("LOD", "high")

    # Find all meshes
    print("\nAll meshes in scene:")
    for mesh in stage.traverse_by_type("Mesh"):
        var xform = mesh.get_world_transform()
        print("  ", mesh.get_path(), " at ", xform.origin)

    # Create new content
    var new_prim = stage.define_prim("/World/NewCube", "Cube")
    new_prim.set_attribute("size", 2.0)
    new_prim.set_local_transform(Transform3D().translated(Vector3(5, 0, 0)))

    # Save changes
    stage.save()
```
