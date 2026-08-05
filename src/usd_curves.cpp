// ---------------------------------------------------------------------------------------------
// Transplanted from godot-usd (https://github.com/migueldeicaza) — MIT License, (c) 2026 Miguel de
// Icaza. Harvested into usd-godot per USD_GODOT_INTEGRATION_PLAN.md (Phase 3), lifted near-verbatim
// and kept in the godot_usd namespace; adapted only as needed for Godot 4.7 / usd-godot. See
// THIRD_PARTY.md for provenance.
// ---------------------------------------------------------------------------------------------

#include "usd_curves.h"

#include "usd_stage_utils.h"

#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/path3d.hpp>
#include <godot_cpp/core/error_macros.hpp>

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/usdGeom/basisCurves.h>
#include <pxr/usd/usdGeom/tokens.h>

namespace godot_usd {

namespace {

Array to_int_array(const VtArray<int> &p_values) {
	Array array;
	for (int value : p_values) {
		array.push_back(value);
	}
	return array;
}

Array to_float_array(const VtArray<float> &p_values) {
	Array array;
	for (float value : p_values) {
		array.push_back(value);
	}
	return array;
}

Ref<Curve3D> build_linear_curve3d(const VtArray<GfVec3f> &p_points, int p_point_offset, int p_point_count, bool p_closed) {
	ERR_FAIL_COND_V(p_point_count < 2, Ref<Curve3D>());

	Ref<Curve3D> curve;
	curve.instantiate();
	for (int i = 0; i < p_point_count; i++) {
		const GfVec3f &point = p_points[p_point_offset + i];
		curve->add_point(Vector3(point[0], point[1], point[2]));
	}
	curve->set_closed(p_closed);
	return curve;
}

Ref<Curve3D> build_bezier_curve3d(const VtArray<GfVec3f> &p_points, int p_point_offset, int p_point_count, bool p_closed) {
	if (p_closed) {
		ERR_FAIL_COND_V((p_point_count % 3) != 0 || p_point_count < 6, Ref<Curve3D>());
	} else {
		ERR_FAIL_COND_V(((p_point_count - 4) % 3) != 0 || p_point_count < 4, Ref<Curve3D>());
	}

	const int anchor_count = p_closed ? (p_point_count / 3) : ((p_point_count + 2) / 3);
	ERR_FAIL_COND_V(anchor_count < 2, Ref<Curve3D>());

	Ref<Curve3D> curve;
	curve.instantiate();

	for (int anchor_index = 0; anchor_index < anchor_count; anchor_index++) {
		const GfVec3f &anchor = p_points[p_point_offset + anchor_index * 3];
		curve->add_point(Vector3(anchor[0], anchor[1], anchor[2]));
	}

	const int segment_count = p_closed ? anchor_count : (anchor_count - 1);
	for (int segment_index = 0; segment_index < segment_count; segment_index++) {
		const int start = p_point_offset + segment_index * 3;
		const int next_anchor_index = (segment_index + 1) % anchor_count;
		const Vector3 anchor = curve->get_point_position(segment_index);
		const Vector3 next_anchor = curve->get_point_position(next_anchor_index);
		const GfVec3f &out_handle = p_points[start + 1];
		const GfVec3f &in_handle = p_points[start + 2];
		curve->set_point_out(segment_index, Vector3(out_handle[0], out_handle[1], out_handle[2]) - anchor);
		curve->set_point_in(next_anchor_index, Vector3(in_handle[0], in_handle[1], in_handle[2]) - next_anchor);
	}

	curve->set_closed(p_closed);
	return curve;
}

} // namespace

Node3D *build_basis_curves_node(const UsdPrim &p_prim, const UsdTimeCode &p_time, Dictionary *r_mapping_notes) {
	UsdGeomBasisCurves usd_curves(p_prim);
	Node3D *basis_root = memnew(Node3D);

	VtArray<GfVec3f> points;
	VtArray<int> curve_vertex_counts;
	if (!usd_curves.GetPointsAttr().Get(&points, p_time) || !usd_curves.GetCurveVertexCountsAttr().Get(&curve_vertex_counts, p_time)) {
		if (r_mapping_notes != nullptr) {
			(*r_mapping_notes)["usd:mapping_status"] = "BasisCurves prim was detected, but its points or curveVertexCounts could not be read.";
		}
		return basis_root;
	}

	TfToken curve_type = UsdGeomTokens->linear;
	usd_curves.GetTypeAttr().Get(&curve_type, p_time);

	TfToken basis = UsdGeomTokens->bezier;
	usd_curves.GetBasisAttr().Get(&basis, p_time);

	TfToken wrap = UsdGeomTokens->nonperiodic;
	usd_curves.GetWrapAttr().Get(&wrap, p_time);

	VtArray<float> widths;
	usd_curves.GetWidthsAttr().Get(&widths, p_time);
	const TfToken widths_interpolation = usd_curves.GetWidthsInterpolation();

	set_usd_metadata(basis_root, "usd:curve_type", to_godot_string(curve_type.GetString()));
	set_usd_metadata(basis_root, "usd:curve_basis", to_godot_string(basis.GetString()));
	set_usd_metadata(basis_root, "usd:curve_wrap", to_godot_string(wrap.GetString()));
	set_usd_metadata(basis_root, "usd:curve_count", (int)curve_vertex_counts.size());
	set_usd_metadata(basis_root, "usd:curve_vertex_counts", to_int_array(curve_vertex_counts));
	if (!widths.empty()) {
		set_usd_metadata(basis_root, "usd:curve_widths", to_float_array(widths));
		set_usd_metadata(basis_root, "usd:curve_widths_interpolation", to_godot_string(widths_interpolation.GetString()));
	}

	if (curve_type != UsdGeomTokens->linear && !(curve_type == UsdGeomTokens->cubic && basis == UsdGeomTokens->bezier)) {
		if (r_mapping_notes != nullptr) {
			(*r_mapping_notes)["usd:mapping_status"] = vformat(
					"BasisCurves type=%s basis=%s is not mapped yet; only linear and cubic bezier curves are imported as Path3D nodes.",
					to_godot_string(curve_type.GetString()),
					to_godot_string(basis.GetString()));
		}
		return basis_root;
	}

	const bool closed = wrap == UsdGeomTokens->periodic;
	int point_offset = 0;
	for (int curve_index = 0; curve_index < (int)curve_vertex_counts.size(); curve_index++) {
		const int curve_point_count = curve_vertex_counts[curve_index];
		if (curve_point_count <= 0 || point_offset + curve_point_count > (int)points.size()) {
			if (r_mapping_notes != nullptr) {
				(*r_mapping_notes)["usd:mapping_status"] = "BasisCurves topology was invalid; generated Path3D children were truncated.";
			}
			break;
		}

		Ref<Curve3D> curve;
		if (curve_type == UsdGeomTokens->linear) {
			curve = build_linear_curve3d(points, point_offset, curve_point_count, closed);
		} else {
			curve = build_bezier_curve3d(points, point_offset, curve_point_count, closed);
		}
		if (curve.is_null()) {
			if (r_mapping_notes != nullptr) {
				(*r_mapping_notes)["usd:mapping_status"] = "BasisCurves topology could not be converted to Curve3D; generated Path3D children were truncated.";
			}
			break;
		}

		Path3D *path = memnew(Path3D);
		path->set_name(curve_vertex_counts.size() == 1 ? String("Path") : vformat("Path%d", curve_index));
		path->set_curve(curve);
		set_usd_metadata(path, "usd:generated_from_basis_curves", true);
		set_usd_metadata(path, "usd:source_prim_path", to_godot_string(p_prim.GetPath().GetString()));
		set_usd_metadata(path, "usd:basis_curve_index", curve_index);
		set_usd_metadata(path, "usd:basis_curve_vertex_count", curve_point_count);
		set_usd_metadata(path, "usd:basis_curve_closed", closed);
		basis_root->add_child(path);

		point_offset += curve_point_count;
	}

	set_usd_metadata(basis_root, "usd:generated_curve_children", basis_root->get_child_count());
	set_usd_metadata(basis_root, "usd:curve_mapping", "path3d_children");
	return basis_root;
}

} // namespace godot_usd
