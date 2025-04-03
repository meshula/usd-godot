# Godot-USD Integration

A plugin that integrates Universal Scene Description (USD) into the Godot Engine, enabling powerful scene description capabilities while maintaining Godot's real-time interactive features.

## Overview

This project bridges two powerful but fundamentally different approaches to scene representation:

- **Godot** uses a scene tree of nodes with class inheritance and real-time signaling
- **USD** uses composition of typed schemas across layers with non-destructive workflows

The integration leverages USD's Hydra rendering system with Scene Index Filters (SIFs) as an intermediary layer, allowing Godot to function as a render delegate.

## Key Features

- Import and export USD assets directly in Godot
- Non-destructive editing with USD's layer-based workflows
- Real-time visualization of complex USD scenes
- Bidirectional property editing between USD and Godot
- Support for USD variants, references, and payloads
- Maintain USD identity across reloads and updates

## Architecture

The implementation consists of these primary components:

1. **OpenUSD Integration Plugin**: C++ plugin that embeds USD libraries in Godot
2. **Scene Index Bridge**: Synchronizes between USD's scene representation and Godot's scene tree 
3. **Godot Render Delegate**: Implements Hydra's render delegate interface for Godot
4. **Editor Extensions**: UI components for USD asset handling and editing
5. **Scripting Interface**: GDScript/C# API for USD interaction

The key insight is using **Scene Index Filters (SIFs)** as an abstraction layer:

- USD scene data flows through Hydra's Scene Index
- Godot references scene elements by path rather than direct USD object references
- A dedicated USD thread isolates USD's memory management and threading model
- Communication happens via message passing rather than shared memory

## Getting Started

### Prerequisites

- Godot Engine 4.x
- OpenUSD libraries (see [Building a Minimal USD](docs/embeddingUSD.md))
- C++17 compatible compiler

### Installation

1. Clone this repository
2. Build the plugin following the instructions in [Building the Plugin](docs/building.md)
3. Add the plugin to your Godot project

### Basic Usage

```gdscript
# Open a USD stage
var stage = UsdStage.new()
stage.open("res://assets/scene.usd")

# Access USD prims
var root_prim = stage.get_default_prim()
var mesh_prim = stage.get_prim_at_path("/World/Mesh")

# Switch variants
if root_prim.has_variant_set("modelingVariant"):
    root_prim.set_variant_selection("modelingVariant", "detailed")
```

## Documentation

- [Technical Design Document](docs/technical-design-doc.md)
- [Embedding USD Guide](docs/embeddingUSD.md)
- [Scene Index Filter Bridge](docs/sif-bridge-doc.md)
- [System Correspondences](docs/correspondences-doc.md)

## Contributing

Contributions are welcome! Please see our [Contributing Guidelines](CONTRIBUTING.md) and [Code of Conduct](CODE_OF_CONDUCT.md).

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- The OpenUSD team at Pixar for creating and open-sourcing USD
- The Godot Engine community for their ongoing support and feedback
