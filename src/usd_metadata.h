// ---------------------------------------------------------------------------------------------
// Transplanted from godot-usd (https://github.com/migueldeicaza) — MIT License, (c) 2026 Miguel de
// Icaza. Harvested per USD_GODOT_INTEGRATION_PLAN.md (Phase 4/composition), lifted near-verbatim.
// Captures composition arcs (references/payloads/inherits/specializes -> usd:* metadata +
// usd:composition_preservation_mode="read_only") and unmapped USD properties on import.
// See THIRD_PARTY.md.
// ---------------------------------------------------------------------------------------------

#pragma once

#include <godot_cpp/classes/object.hpp>

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/timeCode.h>

namespace godot_usd {

using namespace godot;
using namespace pxr;

void store_unmapped_properties(const UsdPrim &p_prim, const UsdTimeCode &p_time, Object *p_target);
void store_composition_arcs(const UsdPrim &p_prim, Object *p_target);

} // namespace godot_usd
