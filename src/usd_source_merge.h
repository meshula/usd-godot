// ---------------------------------------------------------------------------------------------
// Source-preserving ("static-aware") save, transplanted from godot-usd (MIT, (c) 2026 Miguel de
// Icaza) per USD_GODOT_INTEGRATION_PLAN.md (Phase 4c). Copies the original USD layer and authors
// ONLY the supported edits (transforms, scalar/color preview-surface inputs, same-topology mesh
// points, skeleton rest, primary blend-shape weights), warning (with exact strings) for edits it
// cannot safely merge. See THIRD_PARTY.md.
// ---------------------------------------------------------------------------------------------
#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot_usd {

// Copy the source USD layer to the destination and merge supported edits from the loaded+edited
// Godot scene p_root into the copy. Absolute filesystem paths.
godot::Error save_static_imported_data_to_source_copy(godot::Node *p_root, const godot::String &p_source_absolute_path, const godot::String &p_destination_absolute_path);

// Dispatches between composition-preserving export (override prims reauthoring arcs) and the static
// source-copy merge, based on whether the scene carries preserved read-only composition boundaries.
godot::Error save_source_preserving_layer(godot::Node *p_root, const godot::String &p_source_absolute_path, const godot::String &p_destination_absolute_path);

} // namespace godot_usd
