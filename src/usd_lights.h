// ---------------------------------------------------------------------------------------------
// Light dispatch ported from godot-usd (https://github.com/migueldeicaza) — MIT License,
// (c) 2026 Miguel de Icaza. The originals were inline members of godot-usd's scene loader; ported
// here to free functions per USD_GODOT_INTEGRATION_PLAN.md (Phase 3). See THIRD_PARTY.md.
// ---------------------------------------------------------------------------------------------
#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>

namespace godot_usd {

using namespace godot;
using namespace pxr;

// Build a Godot light node for a UsdLux* prim (Distant/Sphere/Rect/Disk/Cylinder), or nullptr if
// p_prim is not a supported light. Rect/Disk map to AreaLight3D when available, else a
// UsdAreaLightProxy fallback. Mapping notes (approximation caveats) are returned via r_mapping_notes.
Node *build_light_node(const UsdStageRefPtr &p_stage, const UsdPrim &p_prim, const UsdTimeCode &p_time, Dictionary *r_mapping_notes);

} // namespace godot_usd
