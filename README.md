# Godot-USD Integration

A GDExtension plugin that integrates Universal Scene Description (USD) into the Godot Engine, enabling import/export of USD assets and direct stage manipulation from GDScript.

## Overview

This project bridges two powerful but fundamentally different approaches to scene representation:

- **Godot** uses a scene tree of nodes with class inheritance and real-time signaling
- **USD** uses composition of typed schemas across layers with non-destructive workflows

The plugin provides:
- **UsdStageProxy** - GDScript wrapper for USD stages
- **UsdPrimProxy** - GDScript wrapper for USD prims with automatic type conversion
- **UsdDocument** - Import/export between USD files and Godot scenes

## Key Features

- Import USD assets (.usd, .usda, .usdc) directly into Godot scenes
- Export Godot scenes to USD format
- Direct USD stage manipulation from GDScript
- Automatic type conversion between USD and Godot types
- Support for USD variants, references, and payloads
- Transform and attribute access with proper coordinate system handling

## Quick Start

### Prerequisites

- Godot Engine 4.2+
- OpenUSD libraries (see [Building a Minimal USD](docs/embeddingUSD.md))
- C++17 compatible compiler

### Installation

1. Clone this repository
2. Build the plugin following the instructions in [Building the Plugin](docs/building.md)
3. Copy `addons/godot-usd` to your Godot project
4. Enable the plugin in Project Settings > Plugins

### GDScript API

```gdscript
# Open and explore a USD stage
var stage = UsdStageProxy.new()
stage.open("res://assets/scene.usda")

# Get the default prim
var root = stage.get_default_prim()
print("Root prim: ", root.get_name())

# Access prims by path
var mesh = stage.get_prim_at_path("/World/MyMesh")
if mesh:
    print("Type: ", mesh.get_type_name())
    print("Position: ", mesh.get_local_transform().origin)

# Read and write attributes
var cube = stage.get_prim_at_path("/World/Cube")
var size = cube.get_attribute("size")
cube.set_attribute("size", 2.0)

# Work with variants
if root.has_variant_set("LOD"):
    root.set_variant_selection("LOD", "high")
    print("Current LOD: ", root.get_variant_selection("LOD"))

# Traverse all prims
for prim in stage.traverse():
    print(prim.get_path(), " -> ", prim.get_type_name())

# Create new USD content
var new_stage = UsdStageProxy.new()
new_stage.create_new("res://output/new_scene.usda")
var xform = new_stage.define_prim("/Root", "Xform")
var child = new_stage.define_prim("/Root/MyCube", "Cube")
child.set_attribute("size", 1.5)
new_stage.save()
```

### Import USD to Godot Scene

```gdscript
var doc = UsdDocument.new()
var state = UsdState.new()
var parent = Node3D.new()
add_child(parent)

var err = doc.import_from_file("res://assets/model.usda", parent, state)
if err == OK:
    print("Import successful!")
```

## Running Tests

The project uses the GUT (Godot Unit Test) framework for testing:

```bash
./run_tests.sh              # Run all tests
./run_tests.sh -v           # Verbose output
./run_tests.sh test_name    # Run specific test file
```

## Documentation

- [GDScript API Reference](docs/api-reference.md) - Complete API documentation for all classes
- [Building the Plugin](docs/building.md) - Build instructions and requirements
- [CLI Development Workflow](docs/cli-development.md) - Testing and debugging with command line
- [Embedding USD Guide](docs/embeddingUSD.md) - How USD is integrated and plugin registration
- [Technical Design Document](docs/technical-design-doc.md) - Architecture and design decisions
- [System Correspondences](docs/correspondences-doc.md) - USD to Godot type mappings

## Project Structure

```
godot-usd/
├── src/                    # C++ source files
│   ├── usd_stage_proxy.*   # UsdStageProxy implementation
│   ├── usd_prim_proxy.*    # UsdPrimProxy implementation
│   ├── usd_document.*      # Import/export implementation
│   └── register_types.cpp  # GDExtension registration
├── lib/                    # Built libraries and USD plugins
│   ├── libgodot-usd.dylib  # GDExtension library
│   └── usd/                # USD plugin resources (required!)
├── addons/godot-usd/       # Godot addon structure
├── tests/                  # GUT test suite
│   ├── unit/               # Unit tests
│   └── fixtures/           # Test USD files
└── docs/                   # Documentation
```

## Contributing

Contributions are welcome! Please see our [Contributing Guidelines](CONTRIBUTING.md) and [Code of Conduct](CODE_OF_CONDUCT.md).

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- The OpenUSD team at Pixar for creating and open-sourcing USD
- The Godot Engine community for their ongoing support and feedback
