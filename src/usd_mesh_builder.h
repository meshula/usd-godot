// ---------------------------------------------------------------------------------------------
// Transplanted from godot-usd (https://github.com/migueldeicaza) — MIT License, (c) 2026 Miguel de
// Icaza. Harvested into usd-godot per USD_GODOT_INTEGRATION_PLAN.md (Phase 2), lifted near-verbatim
// and kept in the godot_usd namespace; adapted only as needed for Godot 4.7 / usd-godot. See
// THIRD_PARTY.md for provenance.
// ---------------------------------------------------------------------------------------------

#pragma once

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/mesh.h>

namespace godot_usd {

using namespace godot;
using namespace pxr;

struct MeshBuildResult {
	Ref<ArrayMesh> mesh;
	Array material_paths;
	Array material_subsets;
	bool has_skinning = false;
};

MeshBuildResult build_polygon_mesh(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdGeomMesh &p_mesh, Dictionary *r_mapping_notes = nullptr);
Node *build_points_instance(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdPrim &p_prim, Dictionary *r_mapping_notes = nullptr);
Node *build_primitive_mesh_instance(const UsdTimeCode &p_time, const UsdPrim &p_prim);

} // namespace godot_usd
