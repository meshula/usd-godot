// ---------------------------------------------------------------------------------------------
// Transplanted from godot-usd (https://github.com/migueldeicaza) — MIT License, (c) 2026 Miguel de
// Icaza. Harvested into usd-godot per USD_GODOT_INTEGRATION_PLAN.md (Phase 2), lifted near-verbatim
// and kept in the godot_usd namespace; adapted only as needed for Godot 4.7 / usd-godot. See
// THIRD_PARTY.md for provenance.
// ---------------------------------------------------------------------------------------------

#pragma once

#include <string>

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/usd/usd/stage.h>

namespace godot_usd {

using namespace godot;
using namespace pxr;

String to_godot_string(const std::string &p_string);
String get_absolute_path(const String &p_path);
bool is_usd_scene_extension(const String &p_extension);
Transform3D gf_matrix_to_transform(const GfMatrix4d &p_matrix);

Dictionary get_usd_metadata(const Object *p_object);
void set_usd_metadata(Object *p_object, const String &p_name, const Variant &p_value);
void set_usd_metadata_entries(Object *p_object, const Dictionary &p_entries);
void mark_owner_recursive(Node *p_node, Node *p_owner);

Dictionary collect_stage_metadata(const UsdStageRefPtr &p_stage);
Dictionary collect_variant_sets(const UsdStageRefPtr &p_stage);
Array collect_variant_context(const Dictionary &p_variant_catalog, const String &p_prim_path);
void apply_variant_selections_to_stage(const UsdStageRefPtr &p_stage, const Dictionary &p_variant_selections);
UsdStageRefPtr open_stage_for_instance(const String &p_source_path, const Dictionary &p_variant_selections = Dictionary());

} // namespace godot_usd
