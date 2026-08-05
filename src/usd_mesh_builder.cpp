// ---------------------------------------------------------------------------------------------
// Transplanted from godot-usd (https://github.com/migueldeicaza) — MIT License, (c) 2026 Miguel de
// Icaza. Harvested into usd-godot per USD_GODOT_INTEGRATION_PLAN.md (Phase 2), lifted near-verbatim
// and kept in the godot_usd namespace; adapted only as needed for Godot 4.7 / usd-godot. See
// THIRD_PARTY.md for provenance.
// ---------------------------------------------------------------------------------------------

#include "usd_mesh_builder.h"

#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdGeom/capsule.h>
#include <pxr/usd/usdGeom/cone.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/cylinder.h>
#include <pxr/usd/usdGeom/gprim.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/points.h>
#include <pxr/usd/usdGeom/plane.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/blendShape.h>

#include "usd_materials.h"
#include "usd_stage_utils.h"

namespace godot_usd {

using namespace godot;
using namespace pxr;

namespace {

struct SurfaceAccumulator {
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedVector2Array uvs;
	PackedColorArray colors;
	PackedInt32Array bones;
	PackedFloat32Array weights;
	PackedInt32Array indices;
	int skin_weight_count = 4;
	Ref<Material> material;
	String usd_material_path;
	String binding_kind;
	bool has_material_binding = false;
	String subset_path;
	String subset_name;
	String family_name;
	String family_type;
	PackedInt32Array authored_face_indices;
	PackedInt32Array authored_point_indices;
};

struct SkinningData {
	VtArray<int> joint_indices_values;
	VtArray<float> joint_weights_values;
	TfToken joint_indices_interpolation = UsdGeomTokens->vertex;
	TfToken joint_weights_interpolation = UsdGeomTokens->vertex;
	int joint_indices_element_size = 0;
	int joint_weights_element_size = 0;
	bool valid = false;
	bool has_authored_joint_indices = false;
	bool has_authored_joint_weights = false;
};

struct InbetweenShapeData {
	String name;
	float weight = 0.0f;
	std::unordered_map<int, Vector3> position_offsets_by_point;
	std::unordered_map<int, Vector3> normal_offsets_by_point;
};

struct BlendShapeData {
	String name;
	String target_path;
	std::unordered_map<int, Vector3> position_offsets_by_point;
	std::unordered_map<int, Vector3> normal_offsets_by_point;
	std::vector<InbetweenShapeData> inbetweens;
	Array inbetweens_metadata;
};

UsdGeomPrimvar find_uv_primvar(const UsdGeomMesh &p_mesh) {
	UsdGeomPrimvarsAPI primvars_api(p_mesh.GetPrim());
	static const TfToken uv_tokens[] = {
		TfToken("st"),
		TfToken("map1"),
		TfToken("UVMap"),
		TfToken("uvmap"),
	};

	for (const TfToken &uv_token : uv_tokens) {
		UsdGeomPrimvar uv_primvar = primvars_api.FindPrimvarWithInheritance(uv_token);
		if (uv_primvar && uv_primvar.HasValue()) {
			return uv_primvar;
		}
	}

	return UsdGeomPrimvar();
}

UsdGeomPrimvar find_normals_primvar(const UsdGeomMesh &p_mesh) {
	UsdGeomPrimvarsAPI primvars_api(p_mesh.GetPrim());
	UsdGeomPrimvar normals_primvar = primvars_api.FindPrimvarWithInheritance(TfToken("normals"));
	if (normals_primvar && normals_primvar.HasValue()) {
		return normals_primvar;
	}

	return UsdGeomPrimvar();
}

template <typename T>
bool read_interpolated_value(const VtArray<T> &p_values, const TfToken &p_interpolation, int p_face_index, int p_face_vertex_index, int p_point_index, T *r_value) {
	ERR_FAIL_NULL_V(r_value, false);
	if (p_values.empty()) {
		return false;
	}

	int value_index = -1;
	if (p_interpolation == UsdGeomTokens->constant) {
		value_index = 0;
	} else if (p_interpolation == UsdGeomTokens->uniform) {
		value_index = p_face_index;
	} else if (p_interpolation == UsdGeomTokens->vertex || p_interpolation == UsdGeomTokens->varying) {
		value_index = p_point_index;
	} else if (p_interpolation == UsdGeomTokens->faceVarying) {
		value_index = p_face_vertex_index;
	}

	if (value_index < 0 || value_index >= (int)p_values.size()) {
		return false;
	}

	*r_value = p_values[value_index];
	return true;
}

int get_interpolated_value_index(const TfToken &p_interpolation, int p_face_index, int p_face_vertex_index, int p_point_index, int p_value_count) {
	int value_index = -1;
	if (p_interpolation == UsdGeomTokens->constant) {
		value_index = 0;
	} else if (p_interpolation == UsdGeomTokens->uniform) {
		value_index = p_face_index;
	} else if (p_interpolation == UsdGeomTokens->faceVarying) {
		value_index = p_face_vertex_index;
	} else {
		value_index = p_point_index;
	}

	if (value_index < 0 || value_index >= p_value_count) {
		return -1;
	}

	return value_index;
}

int get_supported_skin_weight_count(const SkinningData &p_skinning_data) {
	if (!p_skinning_data.valid) {
		return 0;
	}
	return p_skinning_data.joint_indices_element_size > 4 ? 8 : 4;
}

String make_inbetween_blend_shape_channel_name(const String &p_blend_shape_name, const String &p_inbetween_name) {
	return p_blend_shape_name + String("__inbetween__") + p_inbetween_name;
}

SkinningData read_skinning_data(const UsdTimeCode &p_time, const UsdPrim &p_prim, Dictionary *r_mapping_notes) {
	SkinningData skinning_data;

	UsdGeomPrimvarsAPI primvars_api(p_prim);
	UsdGeomPrimvar joint_indices_primvar = primvars_api.FindPrimvarWithInheritance(TfToken("skel:jointIndices"));
	UsdGeomPrimvar joint_weights_primvar = primvars_api.FindPrimvarWithInheritance(TfToken("skel:jointWeights"));

	skinning_data.has_authored_joint_indices = joint_indices_primvar && joint_indices_primvar.GetAttr().HasAuthoredValueOpinion();
	skinning_data.has_authored_joint_weights = joint_weights_primvar && joint_weights_primvar.GetAttr().HasAuthoredValueOpinion();

	const bool has_joint_indices = joint_indices_primvar && joint_indices_primvar.ComputeFlattened(&skinning_data.joint_indices_values, p_time);
	const bool has_joint_weights = joint_weights_primvar && joint_weights_primvar.ComputeFlattened(&skinning_data.joint_weights_values, p_time);

	if (has_joint_indices) {
		skinning_data.joint_indices_interpolation = joint_indices_primvar.GetInterpolation();
		skinning_data.joint_indices_element_size = MAX(joint_indices_primvar.GetElementSize(), 1);
	}
	if (has_joint_weights) {
		skinning_data.joint_weights_interpolation = joint_weights_primvar.GetInterpolation();
		skinning_data.joint_weights_element_size = MAX(joint_weights_primvar.GetElementSize(), 1);
	}

	skinning_data.valid = has_joint_indices && has_joint_weights &&
			skinning_data.joint_indices_element_size == skinning_data.joint_weights_element_size &&
			skinning_data.joint_indices_element_size > 0;

	if ((skinning_data.has_authored_joint_indices || skinning_data.has_authored_joint_weights) && !skinning_data.valid && r_mapping_notes != nullptr) {
		(*r_mapping_notes)["usd:skinning_status"] = "Skel joint influences were authored, but jointIndices/jointWeights could not be paired for import.";
	}

	return skinning_data;
}

std::vector<BlendShapeData> read_point_based_blend_shapes(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdPrim &p_prim, int p_point_count, bool p_supports_normals, Dictionary *r_mapping_notes) {
	std::vector<BlendShapeData> blend_shapes;
	ERR_FAIL_COND_V(p_stage == nullptr, blend_shapes);

	UsdSkelBindingAPI skel_binding_api(p_prim);
	UsdAttribute blend_shapes_attr = skel_binding_api.GetBlendShapesAttr();
	UsdRelationship blend_shape_targets_rel = skel_binding_api.GetBlendShapeTargetsRel();
	if (!blend_shapes_attr || !blend_shape_targets_rel) {
		return blend_shapes;
	}

	VtArray<TfToken> blend_shape_names;
	SdfPathVector blend_shape_targets;
	const bool has_blend_shape_names = blend_shapes_attr.Get(&blend_shape_names, p_time) && !blend_shape_names.empty();
	const bool has_blend_shape_targets = blend_shape_targets_rel.GetTargets(&blend_shape_targets) && !blend_shape_targets.empty();
	if (!has_blend_shape_names || !has_blend_shape_targets) {
		return blend_shapes;
	}

	if ((int)blend_shape_names.size() != (int)blend_shape_targets.size() && r_mapping_notes != nullptr) {
		(*r_mapping_notes)["usd:blend_shape_status"] = "Blend shape names and targets had mismatched counts; only paired entries were imported.";
	}

	const int blend_shape_count = MIN((int)blend_shape_names.size(), (int)blend_shape_targets.size());
	for (int blend_shape_index = 0; blend_shape_index < blend_shape_count; blend_shape_index++) {
		UsdPrim blend_shape_prim = p_stage->GetPrimAtPath(blend_shape_targets[blend_shape_index]);
		if (!blend_shape_prim || !blend_shape_prim.IsA<UsdSkelBlendShape>()) {
			continue;
		}

		UsdSkelBlendShape blend_shape(blend_shape_prim);
		VtArray<GfVec3f> offsets;
		if (!blend_shape.GetOffsetsAttr().Get(&offsets, p_time) || offsets.empty()) {
			continue;
		}

		VtArray<int> point_indices;
		const bool has_point_indices = blend_shape.GetPointIndicesAttr().Get(&point_indices, p_time) && !point_indices.empty();
		if (has_point_indices) {
			if ((int)point_indices.size() != (int)offsets.size()) {
				if (r_mapping_notes != nullptr) {
					(*r_mapping_notes)["usd:blend_shape_status"] = "Blend shape pointIndices did not match offsets; that target was skipped.";
				}
				continue;
			}
		} else if ((int)offsets.size() != p_point_count) {
			if (r_mapping_notes != nullptr) {
				(*r_mapping_notes)["usd:blend_shape_status"] = "Blend shape offsets without pointIndices did not match the mesh point count; that target was skipped.";
			}
			continue;
		}

		BlendShapeData blend_shape_data;
		blend_shape_data.name = to_godot_string(blend_shape_names[blend_shape_index].GetString());
		blend_shape_data.target_path = to_godot_string(blend_shape_targets[blend_shape_index].GetString());

		for (int offset_index = 0; offset_index < (int)offsets.size(); offset_index++) {
			const int point_index = has_point_indices ? point_indices[offset_index] : offset_index;
			if (point_index < 0 || point_index >= p_point_count) {
				continue;
			}
			const GfVec3f &offset = offsets[offset_index];
			blend_shape_data.position_offsets_by_point[point_index] = Vector3(offset[0], offset[1], offset[2]);
		}

		VtArray<GfVec3f> normal_offsets;
		const bool has_normal_offsets = blend_shape.GetNormalOffsetsAttr().Get(&normal_offsets, p_time) && !normal_offsets.empty();
		if (has_normal_offsets) {
			if (!p_supports_normals) {
				if (r_mapping_notes != nullptr) {
					(*r_mapping_notes)["usd:blend_shape_status"] = "Point-based blend shape normalOffsets were authored, but Godot point primitives do not preserve normal deltas.";
				}
			} else if ((int)normal_offsets.size() != (int)offsets.size()) {
				if (r_mapping_notes != nullptr) {
					(*r_mapping_notes)["usd:blend_shape_status"] = "Blend shape normalOffsets did not match offsets; those normal deltas were skipped.";
				}
			} else {
				for (int offset_index = 0; offset_index < (int)normal_offsets.size(); offset_index++) {
					const int point_index = has_point_indices ? point_indices[offset_index] : offset_index;
					if (point_index < 0 || point_index >= p_point_count) {
						continue;
					}
					const GfVec3f &normal_offset = normal_offsets[offset_index];
					blend_shape_data.normal_offsets_by_point[point_index] = Vector3(normal_offset[0], normal_offset[1], normal_offset[2]);
				}
			}
		}

		const std::vector<UsdSkelInbetweenShape> authored_inbetweens = blend_shape.GetAuthoredInbetweens();
		for (const UsdSkelInbetweenShape &inbetween : authored_inbetweens) {
			if (!inbetween) {
				continue;
			}

			InbetweenShapeData inbetween_data;
			Dictionary inbetween_metadata;
			const String attr_name = to_godot_string(inbetween.GetAttr().GetName().GetString());
			inbetween_metadata["attr_name"] = attr_name;

			String display_name = attr_name;
			if (display_name.begins_with("inbetweens:")) {
				display_name = display_name.substr(String("inbetweens:").length());
			}
			inbetween_data.name = display_name;
			inbetween_metadata["name"] = display_name;

			float weight = 0.0f;
			if (inbetween.GetWeight(&weight)) {
				inbetween_data.weight = weight;
				inbetween_metadata["weight"] = weight;
			}

			VtArray<GfVec3f> inbetween_offsets;
			if (inbetween.GetOffsets(&inbetween_offsets)) {
				inbetween_metadata["offset_count"] = (int)inbetween_offsets.size();
				if (has_point_indices) {
					if ((int)inbetween_offsets.size() == (int)point_indices.size()) {
						for (int offset_index = 0; offset_index < (int)inbetween_offsets.size(); offset_index++) {
							const int point_index = point_indices[offset_index];
							if (point_index < 0 || point_index >= p_point_count) {
								continue;
							}
							const GfVec3f &offset = inbetween_offsets[offset_index];
							inbetween_data.position_offsets_by_point[point_index] = Vector3(offset[0], offset[1], offset[2]);
						}
					}
				} else if ((int)inbetween_offsets.size() == p_point_count) {
					for (int offset_index = 0; offset_index < (int)inbetween_offsets.size(); offset_index++) {
						const GfVec3f &offset = inbetween_offsets[offset_index];
						inbetween_data.position_offsets_by_point[offset_index] = Vector3(offset[0], offset[1], offset[2]);
					}
				}
			}

			VtArray<GfVec3f> inbetween_normal_offsets;
			if (inbetween.GetNormalOffsets(&inbetween_normal_offsets)) {
				if (!p_supports_normals) {
					if (r_mapping_notes != nullptr) {
						(*r_mapping_notes)["usd:blend_shape_status"] = "Point-based blend shape inbetween normalOffsets were authored, but Godot point primitives do not preserve normal deltas.";
					}
				} else {
					inbetween_metadata["normal_offset_count"] = (int)inbetween_normal_offsets.size();
					if (has_point_indices) {
						if ((int)inbetween_normal_offsets.size() == (int)point_indices.size()) {
							for (int offset_index = 0; offset_index < (int)inbetween_normal_offsets.size(); offset_index++) {
								const int point_index = point_indices[offset_index];
								if (point_index < 0 || point_index >= p_point_count) {
									continue;
								}
								const GfVec3f &normal_offset = inbetween_normal_offsets[offset_index];
								inbetween_data.normal_offsets_by_point[point_index] = Vector3(normal_offset[0], normal_offset[1], normal_offset[2]);
							}
						}
					} else if ((int)inbetween_normal_offsets.size() == p_point_count) {
						for (int offset_index = 0; offset_index < (int)inbetween_normal_offsets.size(); offset_index++) {
							const GfVec3f &normal_offset = inbetween_normal_offsets[offset_index];
							inbetween_data.normal_offsets_by_point[offset_index] = Vector3(normal_offset[0], normal_offset[1], normal_offset[2]);
						}
					}
				}
			}

			blend_shape_data.inbetweens.push_back(inbetween_data);
			blend_shape_data.inbetweens_metadata.push_back(inbetween_metadata);
		}

		blend_shapes.push_back(blend_shape_data);
	}

	return blend_shapes;
}

TypedArray<Array> build_surface_blend_shapes(const SurfaceAccumulator &p_surface, const std::vector<BlendShapeData> &p_blend_shapes) {
	TypedArray<Array> surface_blend_shapes;
	if (p_blend_shapes.empty()) {
		return surface_blend_shapes;
	}

	for (const BlendShapeData &blend_shape : p_blend_shapes) {
		auto append_blend_shape_surface = [&](const std::unordered_map<int, Vector3> &p_position_offsets_by_point, const std::unordered_map<int, Vector3> &p_normal_offsets_by_point) {
			PackedVector3Array blend_shape_vertices;
			blend_shape_vertices.resize(p_surface.vertices.size());
			PackedVector3Array blend_shape_normals;
			if (!p_surface.normals.is_empty()) {
				blend_shape_normals.resize(p_surface.normals.size());
			}

			for (int vertex_index = 0; vertex_index < p_surface.vertices.size(); vertex_index++) {
				const int point_index = p_surface.authored_point_indices[vertex_index];
				const auto position_it = p_position_offsets_by_point.find(point_index);
				blend_shape_vertices.set(vertex_index, position_it != p_position_offsets_by_point.end() ? position_it->second : Vector3());
				if (!p_surface.normals.is_empty()) {
					const auto normal_it = p_normal_offsets_by_point.find(point_index);
					blend_shape_normals.set(vertex_index, normal_it != p_normal_offsets_by_point.end() ? normal_it->second : Vector3());
				}
			}

			Array blend_shape_arrays;
			blend_shape_arrays.resize(Mesh::ARRAY_MAX);
			blend_shape_arrays[Mesh::ARRAY_VERTEX] = blend_shape_vertices;
			if (!p_surface.normals.is_empty()) {
				blend_shape_arrays[Mesh::ARRAY_NORMAL] = blend_shape_normals;
			}
			surface_blend_shapes.push_back(blend_shape_arrays);
		};

		append_blend_shape_surface(blend_shape.position_offsets_by_point, blend_shape.normal_offsets_by_point);
		for (const InbetweenShapeData &inbetween : blend_shape.inbetweens) {
			append_blend_shape_surface(inbetween.position_offsets_by_point, inbetween.normal_offsets_by_point);
		}
	}

	return surface_blend_shapes;
}

void store_blend_shape_metadata(const std::vector<BlendShapeData> &p_blend_shapes, const String &p_mapping_name, Dictionary *r_mapping_notes) {
	if (r_mapping_notes == nullptr || p_blend_shapes.empty()) {
		return;
	}

	Array blend_shape_names;
	Array blend_shape_targets;
	Dictionary blend_shape_has_normal_offsets;
	Dictionary blend_shape_inbetweens;
	Dictionary blend_shape_channels;
	for (const BlendShapeData &blend_shape : p_blend_shapes) {
		blend_shape_names.push_back(blend_shape.name);
		blend_shape_targets.push_back(blend_shape.target_path);
		blend_shape_has_normal_offsets[blend_shape.name] = !blend_shape.normal_offsets_by_point.empty();
		if (!blend_shape.inbetweens_metadata.is_empty()) {
			blend_shape_inbetweens[blend_shape.name] = blend_shape.inbetweens_metadata;
		}

		Array channel_entries;
		Dictionary primary_channel;
		primary_channel["channel_name"] = blend_shape.name;
		primary_channel["weight"] = 1.0;
		primary_channel["primary"] = true;
		channel_entries.push_back(primary_channel);
		for (const InbetweenShapeData &inbetween : blend_shape.inbetweens) {
			Dictionary inbetween_channel;
			inbetween_channel["channel_name"] = make_inbetween_blend_shape_channel_name(blend_shape.name, inbetween.name);
			inbetween_channel["weight"] = inbetween.weight;
			inbetween_channel["primary"] = false;
			inbetween_channel["name"] = inbetween.name;
			channel_entries.push_back(inbetween_channel);
		}
		blend_shape_channels[blend_shape.name] = channel_entries;
	}

	(*r_mapping_notes)["usd:blend_shape_mapping"] = p_mapping_name;
	(*r_mapping_notes)["usd:blend_shape_names"] = blend_shape_names;
	(*r_mapping_notes)["usd:blend_shape_targets"] = blend_shape_targets;
	(*r_mapping_notes)["usd:blend_shape_has_normal_offsets"] = blend_shape_has_normal_offsets;
	(*r_mapping_notes)["usd:blend_shape_channels"] = blend_shape_channels;
	if (!blend_shape_inbetweens.is_empty()) {
		(*r_mapping_notes)["usd:blend_shape_inbetweens"] = blend_shape_inbetweens;
	}
}

void get_packed_skinning_influences(const SkinningData &p_skinning_data, int p_face_index, int p_face_vertex_index, int p_point_index, int p_max_influences, int *r_bones, float *r_weights) {
	ERR_FAIL_NULL(r_bones);
	ERR_FAIL_NULL(r_weights);
	ERR_FAIL_COND(p_max_influences <= 0);
	for (int influence_index = 0; influence_index < p_max_influences; influence_index++) {
		r_bones[influence_index] = 0;
		r_weights[influence_index] = 0.0f;
	}

	if (!p_skinning_data.valid) {
		return;
	}

	const int joint_indices_value_count = p_skinning_data.joint_indices_values.size() / p_skinning_data.joint_indices_element_size;
	const int joint_weights_value_count = p_skinning_data.joint_weights_values.size() / p_skinning_data.joint_weights_element_size;
	const int joint_indices_value_index = get_interpolated_value_index(p_skinning_data.joint_indices_interpolation, p_face_index, p_face_vertex_index, p_point_index, joint_indices_value_count);
	const int joint_weights_value_index = get_interpolated_value_index(p_skinning_data.joint_weights_interpolation, p_face_index, p_face_vertex_index, p_point_index, joint_weights_value_count);
	if (joint_indices_value_index < 0 || joint_weights_value_index < 0) {
		return;
	}

	struct InfluenceEntry {
		int joint = 0;
		float weight = 0.0f;
	};

	std::vector<InfluenceEntry> influences;
	for (int influence_index = 0; influence_index < p_skinning_data.joint_indices_element_size; influence_index++) {
		const int joint_value_index = joint_indices_value_index * p_skinning_data.joint_indices_element_size + influence_index;
		const int weight_value_index = joint_weights_value_index * p_skinning_data.joint_weights_element_size + influence_index;
		if (joint_value_index >= (int)p_skinning_data.joint_indices_values.size() || weight_value_index >= (int)p_skinning_data.joint_weights_values.size()) {
			break;
		}

		const float weight = p_skinning_data.joint_weights_values[weight_value_index];
		if (weight <= 0.0f) {
			continue;
		}

		InfluenceEntry entry;
		entry.joint = p_skinning_data.joint_indices_values[joint_value_index];
		entry.weight = weight;
		influences.push_back(entry);
	}

	for (int influence_index = 0; influence_index < (int)influences.size(); influence_index++) {
		const InfluenceEntry &entry = influences[influence_index];
		for (int slot = 0; slot < p_max_influences; slot++) {
			if (entry.weight > r_weights[slot]) {
				for (int shift = p_max_influences - 1; shift > slot; shift--) {
					r_bones[shift] = r_bones[shift - 1];
					r_weights[shift] = r_weights[shift - 1];
				}
				r_bones[slot] = entry.joint;
				r_weights[slot] = entry.weight;
				break;
			}
		}
	}

	float total_weight = 0.0f;
	for (int influence_index = 0; influence_index < p_max_influences; influence_index++) {
		total_weight += r_weights[influence_index];
	}
	if (total_weight > 0.0f) {
		for (int influence_index = 0; influence_index < p_max_influences; influence_index++) {
			r_weights[influence_index] /= total_weight;
		}
	}
}

void store_skin_binding_metadata(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdPrim &p_prim, Dictionary *r_mapping_notes) {
	ERR_FAIL_COND(p_stage == nullptr);
	if (r_mapping_notes == nullptr) {
		return;
	}

	UsdSkelBindingAPI skel_binding_api(p_prim);
	UsdSkelSkeleton bound_skeleton = skel_binding_api.GetInheritedSkeleton();
	if (bound_skeleton) {
		(*r_mapping_notes)["usd:skel_skeleton_path"] = to_godot_string(bound_skeleton.GetPath().GetString());
	}

	GfMatrix4d geom_bind_matrix(1.0);
	UsdGeomPrimvar geom_bind_primvar = UsdGeomPrimvarsAPI(p_prim).FindPrimvarWithInheritance(TfToken("skel:geomBindTransform"));
	if (geom_bind_primvar && geom_bind_primvar.Get(&geom_bind_matrix, p_time)) {
		Transform3D stage_correction;
		const TfToken up_axis = UsdGeomGetStageUpAxis(p_stage);
		if (up_axis == UsdGeomTokens->z) {
			stage_correction = Transform3D(Basis(Vector3(1, 0, 0), (real_t)-Math::PI * 0.5).scaled(Vector3(UsdGeomGetStageMetersPerUnit(p_stage), UsdGeomGetStageMetersPerUnit(p_stage), UsdGeomGetStageMetersPerUnit(p_stage))), Vector3());
		} else {
			stage_correction = Transform3D(Basis().scaled(Vector3(UsdGeomGetStageMetersPerUnit(p_stage), UsdGeomGetStageMetersPerUnit(p_stage), UsdGeomGetStageMetersPerUnit(p_stage))), Vector3());
		}
		(*r_mapping_notes)["usd:skel_geom_bind_transform"] = stage_correction * gf_matrix_to_transform(geom_bind_matrix);
	}
}

} // namespace

MeshBuildResult build_polygon_mesh(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdGeomMesh &p_mesh, Dictionary *r_mapping_notes) {
	MeshBuildResult result;
	ERR_FAIL_COND_V(p_stage == nullptr, result);

	VtArray<GfVec3f> points;
	VtArray<int> face_vertex_counts;
	VtArray<int> face_vertex_indices;
	if (!p_mesh.GetPointsAttr().Get(&points, p_time) ||
			!p_mesh.GetFaceVertexCountsAttr().Get(&face_vertex_counts, p_time) ||
			!p_mesh.GetFaceVertexIndicesAttr().Get(&face_vertex_indices, p_time)) {
		return result;
	}

	TfToken orientation = UsdGeomTokens->rightHanded;
	UsdGeomGprim gprim(p_mesh.GetPrim());
	gprim.GetOrientationAttr().Get(&orientation, p_time);

	UsdGeomPrimvar normals_primvar(p_mesh.GetNormalsAttr());
	if (!normals_primvar || !normals_primvar.HasValue()) {
		normals_primvar = find_normals_primvar(p_mesh);
	}
	VtArray<GfVec3f> normals_values;
	TfToken normals_interpolation = UsdGeomTokens->vertex;
	const bool has_normals = normals_primvar && normals_primvar.ComputeFlattened(&normals_values, p_time);
	if (has_normals) {
		normals_interpolation = normals_primvar.GetInterpolation();
	}

	UsdGeomPrimvar uv_primvar = find_uv_primvar(p_mesh);
	VtArray<GfVec2f> uv_values;
	TfToken uv_interpolation = UsdGeomTokens->faceVarying;
	const bool has_uvs = uv_primvar && uv_primvar.ComputeFlattened(&uv_values, p_time);
	if (has_uvs) {
		uv_interpolation = uv_primvar.GetInterpolation();
	}

	UsdGeomPrimvar display_color_primvar = gprim.GetDisplayColorPrimvar();
	VtArray<GfVec3f> display_colors;
	TfToken display_color_interpolation = UsdGeomTokens->constant;
	const bool has_display_color = display_color_primvar && display_color_primvar.ComputeFlattened(&display_colors, p_time);
	if (has_display_color) {
		display_color_interpolation = display_color_primvar.GetInterpolation();
	}

	const SkinningData skinning_data = read_skinning_data(p_time, p_mesh.GetPrim(), r_mapping_notes);
	const int skin_weight_count = get_supported_skin_weight_count(skinning_data);
	const std::vector<BlendShapeData> blend_shapes = read_point_based_blend_shapes(p_stage, p_time, p_mesh.GetPrim(), points.size(), true, r_mapping_notes);
	if (skinning_data.valid) {
		store_skin_binding_metadata(p_stage, p_time, p_mesh.GetPrim(), r_mapping_notes);
		result.has_skinning = true;
	}

	std::vector<SurfaceAccumulator> surfaces;
	surfaces.push_back(SurfaceAccumulator());
	surfaces[0].binding_kind = "mesh";
	surfaces[0].skin_weight_count = skin_weight_count > 0 ? skin_weight_count : 4;

	UsdShadeMaterialBindingAPI mesh_binding_api(p_mesh.GetPrim());
	const UsdRelationship mesh_binding_relationship = p_mesh.GetPrim().GetRelationship(TfToken("material:binding"));
	const bool mesh_has_authored_binding = mesh_binding_relationship && mesh_binding_relationship.HasAuthoredTargets();
	UsdShadeMaterial default_material = mesh_binding_api.ComputeBoundMaterial();
	if (default_material) {
		surfaces[0].material = build_material_from_usd_material(p_stage, p_time, default_material, r_mapping_notes);
		surfaces[0].usd_material_path = to_godot_string(default_material.GetPath().GetString());
		surfaces[0].has_material_binding = mesh_has_authored_binding;
	}

	std::vector<int> face_to_surface(face_vertex_counts.size(), 0);
	const std::vector<UsdGeomSubset> material_subsets = mesh_binding_api.GetMaterialBindSubsets();
	for (const UsdGeomSubset &subset : material_subsets) {
		VtIntArray subset_faces;
		if (!subset.GetIndicesAttr().Get(&subset_faces, p_time)) {
			continue;
		}

		SurfaceAccumulator subset_surface;
		subset_surface.binding_kind = "subset";
		subset_surface.skin_weight_count = skin_weight_count > 0 ? skin_weight_count : 4;
		subset_surface.subset_path = to_godot_string(subset.GetPath().GetString());
		subset_surface.subset_name = to_godot_string(subset.GetPrim().GetName().GetString());
		TfToken family_name;
		subset.GetFamilyNameAttr().Get(&family_name, p_time);
		subset_surface.family_name = to_godot_string(family_name.GetString());
		if (!family_name.IsEmpty()) {
			subset_surface.family_type = to_godot_string(UsdGeomSubset::GetFamilyType(UsdGeomImageable(p_mesh.GetPrim()), family_name).GetString());
		}

		const UsdRelationship subset_binding_relationship = subset.GetPrim().GetRelationship(TfToken("material:binding"));
		const bool subset_has_authored_binding = subset_binding_relationship && subset_binding_relationship.HasAuthoredTargets();
		UsdShadeMaterial subset_material = UsdShadeMaterialBindingAPI(subset.GetPrim()).ComputeBoundMaterial();
		if (subset_material && subset_has_authored_binding) {
			subset_surface.material = build_material_from_usd_material(p_stage, p_time, subset_material, r_mapping_notes);
			subset_surface.usd_material_path = to_godot_string(subset_material.GetPath().GetString());
			subset_surface.has_material_binding = true;
		} else {
			subset_surface.material = surfaces[0].material;
			subset_surface.usd_material_path = surfaces[0].usd_material_path;
		}

		const int surface_index = (int)surfaces.size();
		surfaces.push_back(subset_surface);
		for (int i = 0; i < (int)subset_faces.size(); i++) {
			const int face_index = subset_faces[i];
			if (face_index >= 0 && face_index < (int)face_to_surface.size()) {
				face_to_surface[face_index] = surface_index;
			}
		}
	}

	int face_vertex_cursor = 0;
	for (int face = 0; face < (int)face_vertex_counts.size(); face++) {
		const int count = face_vertex_counts[face];
		if (count < 3 || face_vertex_cursor + count > (int)face_vertex_indices.size()) {
			face_vertex_cursor += count;
			continue;
		}

		SurfaceAccumulator &surface = surfaces[face_to_surface[face]];
		auto append_corner = [&](int p_corner_offset, const Vector3 &p_generated_normal) {
			const int point_index = face_vertex_indices[face_vertex_cursor + p_corner_offset];
			const int face_vertex_index = face_vertex_cursor + p_corner_offset;
			surface.indices.push_back(surface.vertices.size());
			surface.vertices.push_back(Vector3(points[point_index][0], points[point_index][1], points[point_index][2]));
			surface.authored_point_indices.push_back(point_index);

			if (has_normals) {
				GfVec3f normal_value(0.0f);
				if (read_interpolated_value(normals_values, normals_interpolation, face, face_vertex_index, point_index, &normal_value)) {
					surface.normals.push_back(Vector3(normal_value[0], normal_value[1], normal_value[2]));
				} else {
					surface.normals.push_back(p_generated_normal);
				}
			} else {
				surface.normals.push_back(p_generated_normal);
			}

			if (has_uvs) {
				GfVec2f uv_value(0.0f);
				if (read_interpolated_value(uv_values, uv_interpolation, face, face_vertex_index, point_index, &uv_value)) {
					surface.uvs.push_back(Vector2((real_t)uv_value[0], 1.0f - (real_t)uv_value[1]));
				} else {
					surface.uvs.push_back(Vector2());
				}
			}

			if (has_display_color) {
				GfVec3f color_value(1.0f);
				if (read_interpolated_value(display_colors, display_color_interpolation, face, face_vertex_index, point_index, &color_value)) {
					surface.colors.push_back(Color(color_value[0], color_value[1], color_value[2], 1.0f));
				} else {
					surface.colors.push_back(Color(1.0f, 1.0f, 1.0f, 1.0f));
				}
			}

			if (skinning_data.valid) {
				const int old_size = surface.bones.size();
				surface.bones.resize(old_size + surface.skin_weight_count);
				surface.weights.resize(old_size + surface.skin_weight_count);
				int packed_bones[8];
				float packed_weights[8];
				get_packed_skinning_influences(skinning_data, face, face_vertex_index, point_index, surface.skin_weight_count, packed_bones, packed_weights);
				for (int influence_index = 0; influence_index < surface.skin_weight_count; influence_index++) {
					surface.bones.set(old_size + influence_index, packed_bones[influence_index]);
					surface.weights.set(old_size + influence_index, packed_weights[influence_index]);
				}
			}
		};

		for (int corner = 1; corner < count - 1; corner++) {
			const int point0_index = face_vertex_indices[face_vertex_cursor + 0];
			const int point1_index = face_vertex_indices[face_vertex_cursor + corner];
			const int point2_index = face_vertex_indices[face_vertex_cursor + corner + 1];
			const Vector3 point0(points[point0_index][0], points[point0_index][1], points[point0_index][2]);
			const Vector3 point1(points[point1_index][0], points[point1_index][1], points[point1_index][2]);
			const Vector3 point2(points[point2_index][0], points[point2_index][1], points[point2_index][2]);

			Vector3 a = point0;
			Vector3 b = point1;
			Vector3 c = point2;
			if (orientation == UsdGeomTokens->rightHanded) {
				b = point2;
				c = point1;
			}

			Vector3 generated_normal = (b - a).cross(c - a);
			if (generated_normal.length_squared() > 0.0f) {
				generated_normal.normalize();
			}

			append_corner(0, generated_normal);
			if (orientation == UsdGeomTokens->rightHanded) {
				append_corner(corner + 1, generated_normal);
				append_corner(corner, generated_normal);
			} else {
				append_corner(corner, generated_normal);
				append_corner(corner + 1, generated_normal);
			}
			surface.authored_face_indices.push_back(face);
		}
		face_vertex_cursor += count;
	}

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	if (!blend_shapes.empty()) {
		mesh->set_blend_shape_mode(Mesh::BLEND_SHAPE_MODE_RELATIVE);
		for (const BlendShapeData &blend_shape : blend_shapes) {
			mesh->add_blend_shape(blend_shape.name);
			for (const InbetweenShapeData &inbetween : blend_shape.inbetweens) {
				mesh->add_blend_shape(make_inbetween_blend_shape_channel_name(blend_shape.name, inbetween.name));
			}
		}
	}
	for (int i = 0; i < (int)surfaces.size(); i++) {
		const SurfaceAccumulator &surface = surfaces[i];
		if (surface.vertices.is_empty() || surface.indices.is_empty()) {
			continue;
		}

		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = surface.vertices;
		arrays[Mesh::ARRAY_NORMAL] = surface.normals;
		arrays[Mesh::ARRAY_INDEX] = surface.indices;
		if (!surface.uvs.is_empty()) {
			arrays[Mesh::ARRAY_TEX_UV] = surface.uvs;
		}
		if (!surface.colors.is_empty()) {
			arrays[Mesh::ARRAY_COLOR] = surface.colors;
		}
		if (!surface.bones.is_empty()) {
			arrays[Mesh::ARRAY_BONES] = surface.bones;
			arrays[Mesh::ARRAY_WEIGHTS] = surface.weights;
		}
		uint64_t mesh_flags = 0;
		if (surface.skin_weight_count > 4) {
			mesh_flags |= Mesh::ARRAY_FLAG_USE_8_BONE_WEIGHTS;
		}
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays, build_surface_blend_shapes(surface, blend_shapes), Dictionary(), mesh_flags);

		Ref<Material> surface_material = surface.material;
		if (surface_material.is_null() && has_display_color && display_color_interpolation == UsdGeomTokens->constant && !display_colors.empty()) {
			const GfVec3f color_value = display_colors[0];
			surface_material = make_display_color_material(Color(color_value[0], color_value[1], color_value[2], 1.0f), false);
		} else if (surface_material.is_null() && has_display_color) {
			surface_material = make_display_color_material(Color(1.0f, 1.0f, 1.0f, 1.0f), true);
		}
		if (surface_material.is_valid()) {
			mesh->surface_set_material(mesh->get_surface_count() - 1, surface_material);
		}

		if (!surface.usd_material_path.is_empty()) {
			result.material_paths.push_back(surface.usd_material_path);
		}
		Dictionary subset_description;
		subset_description["binding_kind"] = surface.binding_kind;
		subset_description["has_material_binding"] = surface.has_material_binding;
		if (!surface.subset_path.is_empty()) {
			subset_description["subset_path"] = surface.subset_path;
		}
		if (!surface.subset_name.is_empty()) {
			subset_description["subset_name"] = surface.subset_name;
		}
		if (!surface.family_name.is_empty()) {
			subset_description["family_name"] = surface.family_name;
		}
		if (!surface.family_type.is_empty()) {
			subset_description["family_type"] = surface.family_type;
		}
		if (!surface.usd_material_path.is_empty()) {
			subset_description["material_path"] = surface.usd_material_path;
		}
		if (!surface.authored_face_indices.is_empty()) {
			subset_description["authored_face_indices"] = surface.authored_face_indices;
		}
		if (!surface.authored_point_indices.is_empty()) {
			subset_description["authored_point_indices"] = surface.authored_point_indices;
		}
		result.material_subsets.push_back(subset_description);
	}

	store_blend_shape_metadata(blend_shapes, "array_mesh_relative_piecewise", r_mapping_notes);

	result.mesh = mesh->get_surface_count() > 0 ? mesh : Ref<ArrayMesh>();
	return result;
}

Node *build_points_instance(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdPrim &p_prim, Dictionary *r_mapping_notes) {
	ERR_FAIL_COND_V(p_stage == nullptr, nullptr);
	UsdGeomPoints usd_points(p_prim);
	if (!usd_points) {
		return nullptr;
	}

	VtArray<GfVec3f> points;
	if (!usd_points.GetPointsAttr().Get(&points, p_time) || points.empty()) {
		return nullptr;
	}

	UsdGeomGprim gprim(p_prim);
	UsdGeomPrimvar display_color_primvar = gprim.GetDisplayColorPrimvar();
	VtArray<GfVec3f> display_colors;
	TfToken display_color_interpolation = UsdGeomTokens->constant;
	const bool has_display_color = display_color_primvar && display_color_primvar.ComputeFlattened(&display_colors, p_time);
	if (has_display_color) {
		display_color_interpolation = display_color_primvar.GetInterpolation();
	}

	VtArray<float> widths;
	const bool has_widths = usd_points.GetWidthsAttr().Get(&widths, p_time) && !widths.empty();
	const TfToken widths_interpolation = usd_points.GetWidthsInterpolation();

	const SkinningData skinning_data = read_skinning_data(p_time, p_prim, r_mapping_notes);
	const int skin_weight_count = get_supported_skin_weight_count(skinning_data);
	const std::vector<BlendShapeData> blend_shapes = read_point_based_blend_shapes(p_stage, p_time, p_prim, points.size(), false, r_mapping_notes);

	PackedVector3Array vertices;
	vertices.resize(points.size());
	PackedColorArray colors;
	PackedInt32Array bones;
	PackedFloat32Array weights_array;
	if (has_display_color) {
		colors.resize(points.size());
	}
	if (skinning_data.valid) {
		bones.resize(points.size() * skin_weight_count);
		weights_array.resize(points.size() * skin_weight_count);
	}

	for (int point_index = 0; point_index < (int)points.size(); point_index++) {
		const GfVec3f &point = points[point_index];
		vertices.set(point_index, Vector3(point[0], point[1], point[2]));

		if (has_display_color) {
			GfVec3f color_value(1.0f);
			if (read_interpolated_value(display_colors, display_color_interpolation, point_index, point_index, point_index, &color_value)) {
				colors.set(point_index, Color(color_value[0], color_value[1], color_value[2], 1.0f));
			} else {
				colors.set(point_index, Color(1, 1, 1, 1));
			}
		}

		if (skinning_data.valid) {
			int packed_bones[8];
			float packed_weights[8];
			get_packed_skinning_influences(skinning_data, point_index, point_index, point_index, skin_weight_count, packed_bones, packed_weights);
			for (int influence_index = 0; influence_index < skin_weight_count; influence_index++) {
				bones.set(point_index * skin_weight_count + influence_index, packed_bones[influence_index]);
				weights_array.set(point_index * skin_weight_count + influence_index, packed_weights[influence_index]);
			}
		}
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	if (has_display_color) {
		arrays[Mesh::ARRAY_COLOR] = colors;
	}
	if (skinning_data.valid) {
		arrays[Mesh::ARRAY_BONES] = bones;
		arrays[Mesh::ARRAY_WEIGHTS] = weights_array;
		store_skin_binding_metadata(p_stage, p_time, p_prim, r_mapping_notes);
	}

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	if (!blend_shapes.empty()) {
		mesh->set_blend_shape_mode(Mesh::BLEND_SHAPE_MODE_RELATIVE);
		for (const BlendShapeData &blend_shape : blend_shapes) {
			mesh->add_blend_shape(blend_shape.name);
			for (const InbetweenShapeData &inbetween : blend_shape.inbetweens) {
				mesh->add_blend_shape(make_inbetween_blend_shape_channel_name(blend_shape.name, inbetween.name));
			}
		}
	}
	uint64_t mesh_flags = 0;
	if (skin_weight_count > 4) {
		mesh_flags |= Mesh::ARRAY_FLAG_USE_8_BONE_WEIGHTS;
	}
	TypedArray<Array> point_blend_shapes;
	if (!blend_shapes.empty()) {
		for (const BlendShapeData &blend_shape : blend_shapes) {
			auto append_blend_shape_surface = [&](const std::unordered_map<int, Vector3> &p_position_offsets_by_point) {
				PackedVector3Array blend_shape_vertices;
				blend_shape_vertices.resize(vertices.size());
				for (int point_index = 0; point_index < vertices.size(); point_index++) {
					const auto offset_it = p_position_offsets_by_point.find(point_index);
					blend_shape_vertices.set(point_index, offset_it != p_position_offsets_by_point.end() ? offset_it->second : Vector3());
				}

				Array blend_shape_arrays;
				blend_shape_arrays.resize(Mesh::ARRAY_MAX);
				blend_shape_arrays[Mesh::ARRAY_VERTEX] = blend_shape_vertices;
				point_blend_shapes.push_back(blend_shape_arrays);
			};

			append_blend_shape_surface(blend_shape.position_offsets_by_point);
			for (const InbetweenShapeData &inbetween : blend_shape.inbetweens) {
				append_blend_shape_surface(inbetween.position_offsets_by_point);
			}
		}
	}
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_POINTS, arrays, point_blend_shapes, Dictionary(), mesh_flags);

	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	material->set_flag(BaseMaterial3D::FLAG_USE_POINT_SIZE, true);
	if (has_display_color) {
		material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	}

	float point_size = 4.0f;
	if (has_widths) {
		point_size = MAX(widths[0] * 16.0f, 1.0f);
		if (widths.size() > 1 || widths_interpolation != UsdGeomTokens->constant) {
			if (r_mapping_notes != nullptr) {
				(*r_mapping_notes)["usd:points_status"] = "UsdGeomPoints widths were authored per-point, but Godot point rendering currently approximates them with a single point size.";
			}
		}
	}
	material->set_point_size(point_size);
	mesh->surface_set_material(0, material);

	if (r_mapping_notes != nullptr) {
		(*r_mapping_notes)["usd:points_mapping"] = "mesh_points";
		(*r_mapping_notes)["usd:point_count"] = (int)points.size();
	}
	store_blend_shape_metadata(blend_shapes, "array_points_relative_piecewise", r_mapping_notes);

	MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_mesh(mesh);
	return mesh_instance;
}

Node *build_primitive_mesh_instance(const UsdTimeCode &p_time, const UsdPrim &p_prim) {
	Ref<Mesh> mesh;

	if (p_prim.IsA<UsdGeomCube>()) {
		Ref<BoxMesh> box_mesh;
		box_mesh.instantiate();
		double size = 2.0;
		UsdGeomCube(p_prim).GetSizeAttr().Get(&size, p_time);
		box_mesh->set_size(Vector3((real_t)size, (real_t)size, (real_t)size));
		mesh = box_mesh;
	} else if (p_prim.IsA<UsdGeomSphere>()) {
		Ref<SphereMesh> sphere_mesh;
		sphere_mesh.instantiate();
		double radius = 1.0;
		UsdGeomSphere(p_prim).GetRadiusAttr().Get(&radius, p_time);
		sphere_mesh->set_radius((real_t)radius);
		mesh = sphere_mesh;
	} else if (p_prim.IsA<UsdGeomCapsule>()) {
		Ref<CapsuleMesh> capsule_mesh;
		capsule_mesh.instantiate();
		double radius = 1.0;
		double height = 2.0;
		UsdGeomCapsule capsule(p_prim);
		capsule.GetRadiusAttr().Get(&radius, p_time);
		capsule.GetHeightAttr().Get(&height, p_time);
		capsule_mesh->set_radius((real_t)radius);
		capsule_mesh->set_height((real_t)height);
		mesh = capsule_mesh;
	} else if (p_prim.IsA<UsdGeomCylinder>()) {
		Ref<CylinderMesh> cylinder_mesh;
		cylinder_mesh.instantiate();
		double radius = 1.0;
		double height = 2.0;
		UsdGeomCylinder cylinder(p_prim);
		cylinder.GetRadiusAttr().Get(&radius, p_time);
		cylinder.GetHeightAttr().Get(&height, p_time);
		cylinder_mesh->set_top_radius((real_t)radius);
		cylinder_mesh->set_bottom_radius((real_t)radius);
		cylinder_mesh->set_height((real_t)height);
		mesh = cylinder_mesh;
	} else if (p_prim.IsA<UsdGeomCone>()) {
		Ref<CylinderMesh> cone_mesh;
		cone_mesh.instantiate();
		double radius = 1.0;
		double height = 2.0;
		UsdGeomCone cone(p_prim);
		cone.GetRadiusAttr().Get(&radius, p_time);
		cone.GetHeightAttr().Get(&height, p_time);
		cone_mesh->set_top_radius(0.0f);
		cone_mesh->set_bottom_radius((real_t)radius);
		cone_mesh->set_height((real_t)height);
		mesh = cone_mesh;
	} else if (p_prim.IsA<UsdGeomPlane>()) {
		Ref<PlaneMesh> plane_mesh;
		plane_mesh.instantiate();
		plane_mesh->set_size(Vector2(2.0f, 2.0f));
		mesh = plane_mesh;
	}

	if (mesh.is_null()) {
		return nullptr;
	}

	MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_mesh(mesh);
	return mesh_instance;
}

} // namespace godot_usd
