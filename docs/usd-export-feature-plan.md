# USD Import/Export Feature Plan

## Overview

This document outlines the plan for the Godot-USD plugin, which enables bidirectional integration between Godot Engine and Universal Scene Description (USD). The plugin allows users to import USD files into Godot scenes and export Godot scenes to USD files, facilitating workflows with other USD-compatible applications.

## Current State

The Godot-USD plugin has made significant progress with the following components:

- `UsdPlugin`: The main plugin class that registers with Godot's editor
- `UsdDocument`: Handles USD document operations
- `UsdExportSettings`: Manages export settings
- `UsdState`: Maintains state during import/export operations

### Implemented Features

#### Import Features
- USD file import dialog accessible from the editor toolbar
- Basic USD stage loading and traversal
- Conversion of USD prims to Godot nodes:
  - Xform prims → Node3D
  - Scope prims → Node3D
  - Cube prims → MeshInstance3D with BoxMesh
- Transform application from USD to Godot nodes
- Display color support for cubes (primvars:displayColor → StandardMaterial3D)
- Color space handling for materials
- Scene hierarchy preservation
- Saving imported scenes as .tscn files

#### Export Features
- Export dialog accessible from Scene → Export menu
- Basic export settings UI
- Scene traversal for export
- Initial USD stage creation
- Writing USD files to the filesystem

## Implementation Strategy

### Import Enhancement Plan

1. **Support for Additional USD Prim Types**
   - Mesh prims → MeshInstance3D with appropriate mesh data
   - Camera prims → Camera3D
   - Light prims → Various Light3D types
   - Material prims → Proper Godot materials

2. **Advanced Material Support**
   - PBR material conversion
   - Texture handling
   - Shader networks

3. **Animation Support**
   - USD time samples → Godot animation tracks
   - Skeletal animation
   - Blend shapes

4. **References and Variants**
   - Support for USD references
   - Support for USD variants
   - Payload handling

### Export Enhancement Plan

1. **Node Type Mapping**
   - Implement conversion for the following Godot node types:

   | Godot Node Type | USD Prim Type |
   |-----------------|---------------|
   | Node3D          | Xform         |
   | MeshInstance3D  | Mesh          |
   | Camera3D        | Camera        |
   | Light3D         | Light         |
   | AnimationPlayer | Animation     |

2. **Material Export**
   - Convert Godot materials to USD materials
   - Handle textures and shaders
   - Preserve PBR properties

3. **Animation Export**
   - Convert Godot animations to USD time samples
   - Handle skeletal animations
   - Handle blend shapes

4. **Advanced USD Features**
   - Support for creating USD variants
   - Support for creating USD references
   - Support for creating USD payloads

## Implementation Details

### Import Process

The import process follows these steps:

1. User selects a USD file through the import dialog
2. The plugin opens the USD stage
3. The plugin traverses the USD prim hierarchy
4. For each prim, the plugin:
   - Determines the appropriate Godot node type
   - Creates the node
   - Sets node properties based on prim attributes
   - Applies transforms
   - Handles special cases (materials, etc.)
5. The plugin creates a scene with the root node
6. The scene is saved to a .tscn file
7. The scene is opened in the editor

### Export Process

The export process follows these steps:

1. User selects "USD Scene..." from the Scene → Export menu
2. User configures export settings and selects a destination file
3. The plugin traverses the Godot scene tree
4. For each node, the plugin:
   - Determines the appropriate USD prim type
   - Creates the prim
   - Sets prim attributes based on node properties
   - Handles special cases (materials, animations)
5. The plugin applies export settings (binary format, flatten stage, etc.)
6. The plugin writes the USD stage to the specified file path

### Code Organization

To maintain clean and maintainable code, we will follow these guidelines:

1. **Helper Functions for Node Conversion**
   - Use dedicated helper functions for converting different node types to USD prims and also to Godot nodes
   - This improves code readability and makes the conversion process more modular
   - Each helper function should handle a specific node type or conversion task
   - Example: `_convert_box_mesh_to_cube()`, `_convert_mesh_to_usd_mesh()`, etc.

2. **Separation of Concerns**
   - Keep transformation logic separate from geometry conversion
   - Handle materials in dedicated functions
   - Organize animation conversion in its own set of helpers

## Testing Plan

We will test the import/export features with various scenes, including:

- Simple scenes with basic node types
- Complex scenes with multiple node types
- Scenes with animations
- Scenes with materials and textures

We will verify the imported/exported files by:

- Checking round-trip fidelity (Godot → USD → Godot)
- Opening exported USD files in other USD-compatible applications
- Importing USD files from other applications into Godot
- Checking that scene structure, properties, materials, and animations are preserved

## Future Enhancements

After the core import/export features are implemented, we can consider adding:

- Support for more node/prim types
- Support for more USD features (schemas, composition, etc.)
- Advanced material and shader conversion
- Physics properties
- Custom properties and metadata
- USD stage editing within Godot
- Live-linking with other USD applications

## Conclusion

The Godot-USD plugin has made significant progress in enabling bidirectional integration between Godot and USD. By continuing to implement the features outlined in this plan, we will create a robust tool that allows users to seamlessly work with USD files in Godot and export Godot scenes to USD for use in other applications.
