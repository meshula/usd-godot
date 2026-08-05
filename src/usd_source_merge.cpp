// ---------------------------------------------------------------------------------------------
// Source-preserving ("static-aware") save, transplanted from godot-usd (https://github.com/migueldeicaza)
// — MIT License, (c) 2026 Miguel de Icaza. Harvested per USD_GODOT_INTEGRATION_PLAN.md (Phase 4c),
// lifted near-verbatim from the scene-loader monolith's save machinery. See THIRD_PARTY.md.
// ---------------------------------------------------------------------------------------------
#include "usd_source_merge.h"
#include "usd_mesh_builder.h"   // build_polygon_mesh (re-import source mesh for comparison)
#include "usd_stage_utils.h"    // get_usd_metadata, gf_matrix_to_transform, to_godot_string

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/animation.hpp>
#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/classes/animation_library.hpp>
#include <godot_cpp/classes/skeleton3d.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/reference.h>
#include <pxr/usd/sdf/payload.h>
#include <pxr/usd/sdf/layerOffset.h>
#include <pxr/usd/usd/references.h>
#include <pxr/usd/usd/payloads.h>
#include <pxr/usd/usd/inherits.h>
#include <pxr/usd/usd/specializes.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/xformCommonAPI.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdSkel/skeleton.h>
#include <pxr/usd/usdSkel/animation.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/quatf.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3h.h>
#include <pxr/base/tf/token.h>

#include <cmath>

namespace godot_usd {
using namespace godot;
using namespace pxr;

namespace {
// ---- transform helpers (scene_loader:193/208/459) ------------------------------------------
void apply_transform_components(const Vector3 &p_origin, const Quaternion &p_rotation, const Vector3 &p_scale, bool p_visible, UsdGeomXformable p_xformable) {
	if (!p_xformable) {
		return;
	}
	p_xformable.ClearXformOpOrder();
	p_xformable.AddTranslateOp(UsdGeomXformOp::PrecisionDouble).Set(GfVec3d(p_origin.x, p_origin.y, p_origin.z));
	p_xformable.AddOrientOp(UsdGeomXformOp::PrecisionFloat).Set(GfQuatf(p_rotation.w, GfVec3f(p_rotation.x, p_rotation.y, p_rotation.z)));
	p_xformable.AddScaleOp(UsdGeomXformOp::PrecisionFloat).Set(GfVec3f(p_scale.x, p_scale.y, p_scale.z));

	UsdGeomImageable imageable(p_xformable.GetPrim());
	if (imageable && !p_visible) {
		imageable.MakeInvisible();
	}
}

void apply_node3d_transform(const Node3D *p_node, UsdGeomXformable p_xformable) {
	ERR_FAIL_NULL(p_node);
	const Transform3D transform = p_node->get_transform();
	apply_transform_components(transform.origin, transform.basis.get_rotation_quaternion(), transform.basis.get_scale(), p_node->is_visible(), p_xformable);
}

GfMatrix4d transform_to_gf_matrix(const Transform3D &p_transform) {
	GfMatrix4d matrix(1.0);
	const Vector3 x_axis = p_transform.basis.get_column(0);
	const Vector3 y_axis = p_transform.basis.get_column(1);
	const Vector3 z_axis = p_transform.basis.get_column(2);
	matrix[0][0] = x_axis.x;
	matrix[0][1] = x_axis.y;
	matrix[0][2] = x_axis.z;
	matrix[1][0] = y_axis.x;
	matrix[1][1] = y_axis.y;
	matrix[1][2] = y_axis.z;
	matrix[2][0] = z_axis.x;
	matrix[2][1] = z_axis.y;
	matrix[2][2] = z_axis.z;
	matrix[3][0] = p_transform.origin.x;
	matrix[3][1] = p_transform.origin.y;
	matrix[3][2] = p_transform.origin.z;
	return matrix;
}

// ---- copy helper (scene_loader:266) --------------------------------------------------------
Error copy_file_absolute_preserving_contents(const String &p_source_absolute_path, const String &p_destination_absolute_path) {
	if (p_source_absolute_path.simplify_path() == p_destination_absolute_path.simplify_path()) {
		return OK;
	}

	const Error make_dir_error = DirAccess::make_dir_recursive_absolute(p_destination_absolute_path.get_base_dir());
	ERR_FAIL_COND_V_MSG(make_dir_error != OK, make_dir_error, vformat("Failed to create destination directory for USD save: %s", p_destination_absolute_path.get_base_dir()));

	return DirAccess::copy_absolute(p_source_absolute_path, p_destination_absolute_path);
}

// ---- save-side helpers (scene_loader:479-1245) ---------------------------------------------
GfQuatf godot_quat_to_gf_quat(const Quaternion &p_quaternion) {
	return GfQuatf((float)p_quaternion.w, GfVec3f((float)p_quaternion.x, (float)p_quaternion.y, (float)p_quaternion.z));
}

bool gf_matrix_is_equal_approx(const GfMatrix4d &p_left, const GfMatrix4d &p_right) {
	for (int row = 0; row < 4; row++) {
		for (int column = 0; column < 4; column++) {
			if (!Math::is_equal_approx((real_t)p_left[row][column], (real_t)p_right[row][column])) {
				return false;
			}
		}
	}
	return true;
}

bool gf_quat_is_equal_approx(const GfQuatf &p_left, const GfQuatf &p_right) {
	if (!Math::is_equal_approx((real_t)p_left.GetReal(), (real_t)p_right.GetReal())) {
		return false;
	}
	const GfVec3f left_imaginary = p_left.GetImaginary();
	const GfVec3f right_imaginary = p_right.GetImaginary();
	for (int i = 0; i < 3; i++) {
		if (!Math::is_equal_approx((real_t)left_imaginary[i], (real_t)right_imaginary[i])) {
			return false;
		}
	}
	return true;
}

bool gf_vec3f_is_equal_approx(const GfVec3f &p_left, const GfVec3f &p_right) {
	for (int i = 0; i < 3; i++) {
		if (!Math::is_equal_approx((real_t)p_left[i], (real_t)p_right[i])) {
			return false;
		}
	}
	return true;
}

bool gf_vec3h_is_equal_approx(const GfVec3h &p_left, const GfVec3h &p_right) {
	for (int i = 0; i < 3; i++) {
		if (!Math::is_equal_approx((real_t)p_left[i], (real_t)p_right[i])) {
			return false;
		}
	}
	return true;
}

GfVec3f godot_vector_to_gf_vec3f(const Vector3 &p_vector) {
	return GfVec3f((float)p_vector.x, (float)p_vector.y, (float)p_vector.z);
}

GfVec3h godot_vector_to_gf_vec3h(const Vector3 &p_vector) {
	return GfVec3h((float)p_vector.x, (float)p_vector.y, (float)p_vector.z);
}

String node_path_bone_name(const NodePath &p_path) {
	const String path_string = String(p_path);
	const int colon = path_string.rfind(":");
	if (colon < 0 || colon + 1 >= path_string.length()) {
		return String();
	}
	return path_string.substr(colon + 1);
}

String joint_leaf_name_for_save(const String &p_joint_path) {
	const int slash = p_joint_path.rfind("/");
	if (slash < 0 || slash + 1 >= p_joint_path.length()) {
		return p_joint_path;
	}
	return p_joint_path.substr(slash + 1);
}

String node_path_property_name(const NodePath &p_path) {
	const String path_string = String(p_path);
	const int colon = path_string.rfind(":");
	if (colon < 0 || colon + 1 >= path_string.length()) {
		return String();
	}
	return path_string.substr(colon + 1);
}

bool apply_skeleton_rest_edits_to_stage(Node *p_node, const UsdStageRefPtr &p_stage) {
	ERR_FAIL_NULL_V(p_node, false);
	ERR_FAIL_COND_V(!p_stage, false);

	bool applied_any = false;
	if (Skeleton3D *skeleton = Object::cast_to<Skeleton3D>(p_node)) {
		const Dictionary metadata = get_usd_metadata(skeleton);
		const String prim_path = metadata.get("usd:prim_path", String());
		if (!prim_path.is_empty()) {
			const UsdPrim prim = p_stage->GetPrimAtPath(SdfPath(prim_path.utf8().get_data()));
			UsdSkelSkeleton usd_skeleton(prim);
			if (usd_skeleton) {
				VtArray<TfToken> joints;
				if (usd_skeleton.GetJointsAttr().Get(&joints) && !joints.empty()) {
					VtArray<GfMatrix4d> rest_transforms;
					if (!usd_skeleton.GetRestTransformsAttr().Get(&rest_transforms) || rest_transforms.size() != joints.size()) {
						rest_transforms.resize(joints.size());
					}
					bool changed = false;
					for (int bone_index = 0; bone_index < skeleton->get_bone_count(); bone_index++) {
						int joint_index = bone_index;
						if (skeleton->has_bone_meta(bone_index, StringName("usd_joint_index"))) {
							joint_index = (int)skeleton->get_bone_meta(bone_index, StringName("usd_joint_index"));
						}
						if (joint_index < 0 || joint_index >= (int)joints.size()) {
							continue;
						}
						const GfMatrix4d edited_rest_transform = transform_to_gf_matrix(skeleton->get_bone_rest(bone_index));
						if (!gf_matrix_is_equal_approx(rest_transforms[joint_index], edited_rest_transform)) {
							rest_transforms[joint_index] = edited_rest_transform;
							changed = true;
						}
					}
					if (changed) {
						usd_skeleton.GetRestTransformsAttr().Set(rest_transforms);
						applied_any = true;
					}
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(false); i++) {
		applied_any = apply_skeleton_rest_edits_to_stage(p_node->get_child(i, false), p_stage) || applied_any;
	}

	return applied_any;
}

bool apply_node3d_transform_edits_to_stage(Node *p_node, const UsdStageRefPtr &p_stage) {
	ERR_FAIL_NULL_V(p_node, false);
	ERR_FAIL_COND_V(!p_stage, false);

	bool applied_any = false;
	if (Node3D *node_3d = Object::cast_to<Node3D>(p_node)) {
		const Dictionary metadata = get_usd_metadata(node_3d);
		const String prim_path = metadata.get("usd:prim_path", String());
		if (!prim_path.is_empty() && !(bool)metadata.get("usd:generated_preview", false)) {
			const UsdPrim prim = p_stage->GetPrimAtPath(SdfPath(prim_path.utf8().get_data()));
			UsdGeomXformable xformable(prim);
			if (xformable) {
				GfMatrix4d source_local_matrix(1.0);
				bool resets_xform_stack = false;
				xformable.GetLocalTransformation(&source_local_matrix, &resets_xform_stack, UsdTimeCode::Default());

				const GfMatrix4d edited_local_matrix = transform_to_gf_matrix(node_3d->get_transform());
				if (!gf_matrix_is_equal_approx(source_local_matrix, edited_local_matrix)) {
					apply_node3d_transform(node_3d, xformable);
					applied_any = true;
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(false); i++) {
		applied_any = apply_node3d_transform_edits_to_stage(p_node->get_child(i, false), p_stage) || applied_any;
	}

	return applied_any;
}

bool set_preview_surface_color_input_if_changed(UsdShadeShader p_shader, const TfToken &p_input_name, const Color &p_color) {
	if (!p_shader) {
		return false;
	}

	UsdShadeInput input = p_shader.GetInput(p_input_name);
	if (input && input.HasConnectedSource()) {
		return false;
	}

	const GfVec3f edited_value((float)p_color.r, (float)p_color.g, (float)p_color.b);
	GfVec3f source_value(1.0f, 1.0f, 1.0f);
	if (input && input.Get(&source_value) && gf_vec3f_is_equal_approx(source_value, edited_value)) {
		return false;
	}

	p_shader.CreateInput(p_input_name, SdfValueTypeNames->Color3f).Set(edited_value);
	return true;
}

bool set_preview_surface_float_input_if_changed(UsdShadeShader p_shader, const TfToken &p_input_name, float p_value, float p_default_value) {
	if (!p_shader) {
		return false;
	}

	UsdShadeInput input = p_shader.GetInput(p_input_name);
	if (input && input.HasConnectedSource()) {
		return false;
	}

	float source_value = p_default_value;
	if (input && input.Get(&source_value) && Math::is_equal_approx((real_t)source_value, (real_t)p_value)) {
		return false;
	}

	p_shader.CreateInput(p_input_name, SdfValueTypeNames->Float).Set(p_value);
	return true;
}

bool apply_preview_surface_material_edits_to_bound_material(const UsdShadeMaterial &p_material, const Ref<Material> &p_godot_material) {
	if (!p_material || p_godot_material.is_null()) {
		return false;
	}

	Ref<BaseMaterial3D> base_material = p_godot_material;
	if (base_material.is_null()) {
		return false;
	}

	UsdShadeShader preview_surface = UsdShadeShader::Get(p_material.GetPrim().GetStage(), p_material.GetPath().AppendChild(TfToken("PreviewSurface")));
	if (!preview_surface) {
		return false;
	}

	bool changed = false;
	const Color albedo = base_material->get_albedo();
	changed = set_preview_surface_color_input_if_changed(preview_surface, TfToken("diffuseColor"), albedo) || changed;
	changed = set_preview_surface_float_input_if_changed(preview_surface, TfToken("metallic"), base_material->get_metallic(), 0.0f) || changed;
	changed = set_preview_surface_float_input_if_changed(preview_surface, TfToken("roughness"), base_material->get_roughness(), 0.5f) || changed;
	changed = set_preview_surface_float_input_if_changed(preview_surface, TfToken("opacity"), CLAMP((float)albedo.a, 0.0f, 1.0f), 1.0f) || changed;
	return changed;
}

void append_static_save_warning(PackedStringArray *r_warnings, const String &p_warning) {
	if (r_warnings == nullptr || p_warning.is_empty()) {
		return;
	}
	for (int i = 0; i < r_warnings->size(); i++) {
		if ((*r_warnings)[i] == p_warning) {
			return;
		}
	}
	r_warnings->push_back(p_warning);
}

bool packed_int32_arrays_equal(const PackedInt32Array &p_left, const PackedInt32Array &p_right) {
	if (p_left.size() != p_right.size()) {
		return false;
	}
	for (int i = 0; i < p_left.size(); i++) {
		if (p_left[i] != p_right[i]) {
			return false;
		}
	}
	return true;
}

bool packed_vector2_arrays_equal_approx(const PackedVector2Array &p_left, const PackedVector2Array &p_right) {
	if (p_left.size() != p_right.size()) {
		return false;
	}
	for (int i = 0; i < p_left.size(); i++) {
		if (!p_left[i].is_equal_approx(p_right[i])) {
			return false;
		}
	}
	return true;
}

bool packed_vector3_arrays_equal_approx(const PackedVector3Array &p_left, const PackedVector3Array &p_right) {
	if (p_left.size() != p_right.size()) {
		return false;
	}
	for (int i = 0; i < p_left.size(); i++) {
		if (!p_left[i].is_equal_approx(p_right[i])) {
			return false;
		}
	}
	return true;
}

bool packed_color_arrays_equal_approx(const PackedColorArray &p_left, const PackedColorArray &p_right) {
	if (p_left.size() != p_right.size()) {
		return false;
	}
	for (int i = 0; i < p_left.size(); i++) {
		if (!p_left[i].is_equal_approx(p_right[i])) {
			return false;
		}
	}
	return true;
}

bool packed_byte_arrays_equal(const PackedByteArray &p_left, const PackedByteArray &p_right) {
	if (p_left.size() != p_right.size()) {
		return false;
	}
	for (int i = 0; i < p_left.size(); i++) {
		if (p_left[i] != p_right[i]) {
			return false;
		}
	}
	return true;
}

bool texture_images_equal(const Ref<Texture2D> &p_left, const Ref<Texture2D> &p_right) {
	if (p_left.is_null() && p_right.is_null()) {
		return true;
	}
	if (p_left.is_null() || p_right.is_null()) {
		return false;
	}

	Ref<Image> left_image = p_left->get_image();
	Ref<Image> right_image = p_right->get_image();
	if (left_image.is_null() && right_image.is_null()) {
		return true;
	}
	if (left_image.is_null() || right_image.is_null()) {
		return false;
	}
	if (left_image->get_width() != right_image->get_width() ||
			left_image->get_height() != right_image->get_height() ||
			left_image->get_format() != right_image->get_format()) {
		return false;
	}
	return packed_byte_arrays_equal(left_image->get_data(), right_image->get_data());
}

bool colors_equal_approx(const Color &p_left, const Color &p_right) {
	return p_left.is_equal_approx(p_right);
}

bool texture_network_material_differs(const Ref<Material> &p_current_material, const Ref<Material> &p_source_material) {
	Ref<BaseMaterial3D> current = p_current_material;
	Ref<BaseMaterial3D> source = p_source_material;
	if (current.is_null() || source.is_null()) {
		Dictionary current_sources;
		Dictionary source_sources;
		if (current.is_valid()) {
			current_sources = get_usd_metadata(current.ptr()).get("usd:preview_surface_texture_sources", Dictionary());
		}
		if (source.is_valid()) {
			source_sources = get_usd_metadata(source.ptr()).get("usd:preview_surface_texture_sources", Dictionary());
		}
		return (!current_sources.is_empty() || !source_sources.is_empty()) && current != source;
	}

	const Dictionary source_texture_sources = get_usd_metadata(source.ptr()).get("usd:preview_surface_texture_sources", Dictionary());
	const Dictionary current_texture_sources = get_usd_metadata(current.ptr()).get("usd:preview_surface_texture_sources", Dictionary());
	if (source_texture_sources.is_empty() && current_texture_sources.is_empty()) {
		return false;
	}

	if (!texture_images_equal(current->get_texture(BaseMaterial3D::TEXTURE_ALBEDO), source->get_texture(BaseMaterial3D::TEXTURE_ALBEDO)) ||
			!texture_images_equal(current->get_texture(BaseMaterial3D::TEXTURE_METALLIC), source->get_texture(BaseMaterial3D::TEXTURE_METALLIC)) ||
			!texture_images_equal(current->get_texture(BaseMaterial3D::TEXTURE_ROUGHNESS), source->get_texture(BaseMaterial3D::TEXTURE_ROUGHNESS)) ||
			!texture_images_equal(current->get_texture(BaseMaterial3D::TEXTURE_NORMAL), source->get_texture(BaseMaterial3D::TEXTURE_NORMAL)) ||
			!texture_images_equal(current->get_texture(BaseMaterial3D::TEXTURE_EMISSION), source->get_texture(BaseMaterial3D::TEXTURE_EMISSION)) ||
			!texture_images_equal(current->get_texture(BaseMaterial3D::TEXTURE_CLEARCOAT), source->get_texture(BaseMaterial3D::TEXTURE_CLEARCOAT)) ||
			!texture_images_equal(current->get_texture(BaseMaterial3D::TEXTURE_AMBIENT_OCCLUSION), source->get_texture(BaseMaterial3D::TEXTURE_AMBIENT_OCCLUSION))) {
		return true;
	}

	if (current->get_metallic_texture_channel() != source->get_metallic_texture_channel() ||
			current->get_roughness_texture_channel() != source->get_roughness_texture_channel() ||
			current->get_ao_texture_channel() != source->get_ao_texture_channel() ||
			!current->get_uv1_scale().is_equal_approx(source->get_uv1_scale()) ||
			!current->get_uv1_offset().is_equal_approx(source->get_uv1_offset())) {
		return true;
	}

	if (source_texture_sources.has("diffuseColor") && !colors_equal_approx(current->get_albedo(), source->get_albedo())) {
		return true;
	}
	if (source_texture_sources.has("metallic") && !Math::is_equal_approx(current->get_metallic(), source->get_metallic())) {
		return true;
	}
	if (source_texture_sources.has("roughness") && !Math::is_equal_approx(current->get_roughness(), source->get_roughness())) {
		return true;
	}
	if (source_texture_sources.has("emissiveColor") && !colors_equal_approx(current->get_emission(), source->get_emission())) {
		return true;
	}
	if ((source_texture_sources.has("clearcoat") || source_texture_sources.has("clearcoatRoughness")) &&
			(!Math::is_equal_approx(current->get_clearcoat(), source->get_clearcoat()) ||
					!Math::is_equal_approx(current->get_clearcoat_roughness(), source->get_clearcoat_roughness()))) {
		return true;
	}

	return false;
}

Ref<Material> get_effective_surface_material(MeshInstance3D *p_mesh_instance, const Ref<ArrayMesh> &p_mesh, int p_surface_index) {
	ERR_FAIL_NULL_V(p_mesh_instance, Ref<Material>());
	Ref<Material> material = p_mesh_instance->get_material_override();
	if (material.is_valid()) {
		return material;
	}
	if (p_mesh.is_valid() && p_surface_index >= 0 && p_surface_index < p_mesh->get_surface_count()) {
		return p_mesh->surface_get_material(p_surface_index);
	}
	return Ref<Material>();
}

String get_surface_material_path(const Ref<Material> &p_material) {
	if (p_material.is_null()) {
		return String();
	}
	return get_usd_metadata(p_material.ptr()).get("usd:material_path", String());
}

Dictionary get_surface_description(const Array &p_descriptions, int p_surface_index) {
	if (p_surface_index < 0 || p_surface_index >= p_descriptions.size() || p_descriptions[p_surface_index].get_type() != Variant::DICTIONARY) {
		return Dictionary();
	}
	return p_descriptions[p_surface_index];
}

void append_mesh_array_unsupported_warning(const String &p_prim_path, int p_surface_index, const String &p_kind, PackedStringArray *r_warnings) {
	append_static_save_warning(r_warnings, vformat("%s changed at %s surface %d; source-aware static save currently only merges point positions", p_kind, p_prim_path, p_surface_index));
}

void warn_unsupported_static_mesh_edits(Node *p_node, const UsdStageRefPtr &p_stage, PackedStringArray *r_warnings) {
	ERR_FAIL_NULL(p_node);
	ERR_FAIL_COND(p_stage == nullptr);

	if (MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(p_node)) {
		const Dictionary metadata = get_usd_metadata(mesh_instance);
		const String prim_path = metadata.get("usd:prim_path", String());
		Ref<ArrayMesh> current_mesh = mesh_instance->get_mesh();
		if (!prim_path.is_empty() && current_mesh.is_valid()) {
			UsdGeomMesh usd_mesh(p_stage->GetPrimAtPath(SdfPath(prim_path.utf8().get_data())));
			if (usd_mesh) {
				Dictionary source_mapping_notes;
				const MeshBuildResult source_result = build_polygon_mesh(p_stage, UsdTimeCode::Default(), usd_mesh, &source_mapping_notes);
				Ref<ArrayMesh> source_mesh = source_result.mesh;
				if (source_mesh.is_valid()) {
					if (current_mesh->get_surface_count() != source_mesh->get_surface_count()) {
						append_static_save_warning(r_warnings, vformat("mesh surface topology changed at %s: source had %d surfaces but edited mesh has %d", prim_path, source_mesh->get_surface_count(), current_mesh->get_surface_count()));
					}

					const int surface_count = MIN(current_mesh->get_surface_count(), source_mesh->get_surface_count());
					for (int surface_index = 0; surface_index < surface_count; surface_index++) {
						const Array current_arrays = current_mesh->surface_get_arrays(surface_index);
						const Array source_arrays = source_mesh->surface_get_arrays(surface_index);
						if (current_arrays.size() != Mesh::ARRAY_MAX || source_arrays.size() != Mesh::ARRAY_MAX) {
							continue;
						}

						const PackedInt32Array current_indices = current_arrays[Mesh::ARRAY_INDEX];
						const PackedInt32Array source_indices = source_arrays[Mesh::ARRAY_INDEX];
						if (!packed_int32_arrays_equal(current_indices, source_indices)) {
							append_mesh_array_unsupported_warning(prim_path, surface_index, "mesh index buffer", r_warnings);
						}

						const PackedVector3Array current_normals = current_arrays[Mesh::ARRAY_NORMAL];
						const PackedVector3Array source_normals = source_arrays[Mesh::ARRAY_NORMAL];
						if (!packed_vector3_arrays_equal_approx(current_normals, source_normals)) {
							append_mesh_array_unsupported_warning(prim_path, surface_index, "mesh normal primvar", r_warnings);
						}

						const PackedVector2Array current_uvs = current_arrays[Mesh::ARRAY_TEX_UV];
						const PackedVector2Array source_uvs = source_arrays[Mesh::ARRAY_TEX_UV];
						if (!packed_vector2_arrays_equal_approx(current_uvs, source_uvs)) {
							append_mesh_array_unsupported_warning(prim_path, surface_index, "mesh UV primvar", r_warnings);
						}

						const PackedColorArray current_colors = current_arrays[Mesh::ARRAY_COLOR];
						const PackedColorArray source_colors = source_arrays[Mesh::ARRAY_COLOR];
						if (!packed_color_arrays_equal_approx(current_colors, source_colors)) {
							append_mesh_array_unsupported_warning(prim_path, surface_index, "mesh displayColor primvar", r_warnings);
						}

						const Dictionary source_description = get_surface_description(source_result.material_subsets, surface_index);
						const String source_material_path = source_description.get("material_path", String());
						const Ref<Material> current_material = get_effective_surface_material(mesh_instance, current_mesh, surface_index);
						const Ref<Material> source_material = source_mesh->surface_get_material(surface_index);
						const String current_material_path = get_surface_material_path(current_material);
						if (!source_material_path.is_empty() && (current_material.is_null() || current_material_path != source_material_path)) {
							append_static_save_warning(r_warnings, vformat("material subset rebinding changed at %s surface %d; source-aware static save currently preserves authored subset bindings", prim_path, surface_index));
						}
						if (texture_network_material_differs(current_material, source_material)) {
							append_static_save_warning(r_warnings, vformat("texture-network material edit changed at %s surface %d; source-aware static save currently preserves authored texture networks", prim_path, surface_index));
						}
					}
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(false); i++) {
		warn_unsupported_static_mesh_edits(p_node->get_child(i, false), p_stage, r_warnings);
	}
}

bool apply_material_edits_to_stage(Node *p_node, const UsdStageRefPtr &p_stage, PackedStringArray *r_warnings) {
	ERR_FAIL_NULL_V(p_node, false);
	ERR_FAIL_COND_V(!p_stage, false);

	bool applied_any = false;
	if (MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(p_node)) {
		const Dictionary metadata = get_usd_metadata(mesh_instance);
		const String prim_path = metadata.get("usd:prim_path", String());
		if (!prim_path.is_empty()) {
			const UsdPrim prim = p_stage->GetPrimAtPath(SdfPath(prim_path.utf8().get_data()));
			if (prim) {
				UsdShadeMaterial bound_material = UsdShadeMaterialBindingAPI(prim).ComputeBoundMaterial();
				Ref<Material> material = mesh_instance->get_material_override();
				if (material.is_null()) {
					material = mesh_instance->get_active_material(0);
				}
				if (material.is_valid() && bound_material) {
					const String material_path = get_usd_metadata(material.ptr()).get("usd:material_path", String());
					const String bound_material_path = to_godot_string(bound_material.GetPath().GetString());
					if (material_path.is_empty() || material_path != bound_material_path) {
						append_static_save_warning(r_warnings, vformat("material replacement/rebinding changed at %s; source-aware static save currently only merges in-place edits to the originally bound material", prim_path));
					} else {
						applied_any = apply_preview_surface_material_edits_to_bound_material(bound_material, material) || applied_any;
					}
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(false); i++) {
		applied_any = apply_material_edits_to_stage(p_node->get_child(i, false), p_stage, r_warnings) || applied_any;
	}

	return applied_any;
}

bool apply_mesh_point_edits_to_stage(Node *p_node, const UsdStageRefPtr &p_stage, PackedStringArray *r_warnings) {
	ERR_FAIL_NULL_V(p_node, false);
	ERR_FAIL_COND_V(!p_stage, false);

	bool applied_any = false;
	if (MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(p_node)) {
		const Dictionary metadata = get_usd_metadata(mesh_instance);
		const String prim_path = metadata.get("usd:prim_path", String());
		Ref<ArrayMesh> array_mesh = mesh_instance->get_mesh();
		if (!prim_path.is_empty() && array_mesh.is_valid()) {
			UsdGeomMesh usd_mesh(p_stage->GetPrimAtPath(SdfPath(prim_path.utf8().get_data())));
			if (usd_mesh) {
				VtArray<GfVec3f> points;
				if (usd_mesh.GetPointsAttr().Get(&points) && !points.empty()) {
					std::vector<bool> seen_points(points.size(), false);
					std::vector<GfVec3f> edited_points(points.begin(), points.end());
					bool can_merge = true;
					String unsupported_reason;
					int mapped_vertex_count = 0;
					const Array surface_descriptions = metadata.get("usd:material_subsets", Array());

					for (int surface_index = 0; surface_index < array_mesh->get_surface_count() && can_merge; surface_index++) {
						const Array arrays = array_mesh->surface_get_arrays(surface_index);
						if (arrays.size() != Mesh::ARRAY_MAX) {
							continue;
						}
						const PackedVector3Array vertices = arrays[Mesh::ARRAY_VERTEX];
						if (vertices.is_empty()) {
							continue;
						}

						PackedInt32Array authored_point_indices;
						if (surface_index < surface_descriptions.size() && surface_descriptions[surface_index].get_type() == Variant::DICTIONARY) {
							const Dictionary description = surface_descriptions[surface_index];
							authored_point_indices = description.get("authored_point_indices", PackedInt32Array());
						}

						if (authored_point_indices.size() != vertices.size()) {
							if (array_mesh->get_surface_count() == 1 && vertices.size() == (int)points.size()) {
								authored_point_indices.resize(vertices.size());
								for (int i = 0; i < vertices.size(); i++) {
									authored_point_indices.set(i, i);
								}
							} else {
								unsupported_reason = vformat("mesh topology changed at %s: surface %d has %d vertices, but source point mapping has %d entries", prim_path, surface_index, vertices.size(), authored_point_indices.size());
								can_merge = false;
								break;
							}
						}

						for (int vertex_index = 0; vertex_index < vertices.size(); vertex_index++) {
							const int point_index = authored_point_indices[vertex_index];
							if (point_index < 0 || point_index >= (int)points.size()) {
								unsupported_reason = vformat("mesh point mapping changed at %s: surface %d references invalid source point %d", prim_path, surface_index, point_index);
								can_merge = false;
								break;
							}
							const Vector3 vertex = vertices[vertex_index];
							const GfVec3f edited_point((float)vertex.x, (float)vertex.y, (float)vertex.z);
							if (seen_points[point_index] && !gf_vec3f_is_equal_approx(edited_points[point_index], edited_point)) {
								unsupported_reason = vformat("mesh topology/vertex split changed at %s: source point %d maps to conflicting edited vertices", prim_path, point_index);
								can_merge = false;
								break;
							}
							seen_points[point_index] = true;
							edited_points[point_index] = edited_point;
							mapped_vertex_count++;
						}
					}

					if (can_merge && mapped_vertex_count > 0) {
						bool changed = false;
						for (int point_index = 0; point_index < (int)points.size(); point_index++) {
							if (seen_points[point_index] && !gf_vec3f_is_equal_approx(points[point_index], edited_points[point_index])) {
								points[point_index] = edited_points[point_index];
								changed = true;
							}
						}
						if (changed) {
							usd_mesh.GetPointsAttr().Set(points);
							applied_any = true;
						}
					} else if (!unsupported_reason.is_empty()) {
						append_static_save_warning(r_warnings, unsupported_reason);
					}
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(false); i++) {
		applied_any = apply_mesh_point_edits_to_stage(p_node->get_child(i, false), p_stage, r_warnings) || applied_any;
	}

	return applied_any;
}

bool apply_animation_edits_to_stage(Node *p_node, const UsdStageRefPtr &p_stage, PackedStringArray *r_warnings) {
	ERR_FAIL_NULL_V(p_node, false);
	ERR_FAIL_COND_V(!p_stage, false);

	bool applied_any = false;
	if (AnimationPlayer *player = Object::cast_to<AnimationPlayer>(p_node)) {
		const PackedStringArray animation_names = player->get_animation_list();
		for (int animation_index = 0; animation_index < animation_names.size(); animation_index++) {
			const Ref<Animation> animation = player->get_animation(StringName(animation_names[animation_index]));
			if (animation.is_null()) {
				continue;
			}

			const Dictionary metadata = get_usd_metadata(animation.ptr());
			const String animation_prim_path = metadata.get("usd:animation_prim_path", String());
			const Array joint_paths = metadata.get("usd:joint_paths", Array());
			if (animation_prim_path.is_empty()) {
				continue;
			}

			UsdSkelAnimation usd_animation(p_stage->GetPrimAtPath(SdfPath(animation_prim_path.utf8().get_data())));
			if (!usd_animation) {
				continue;
			}

			UsdAttribute translations_attr = usd_animation.GetTranslationsAttr();
			UsdAttribute rotations_attr = usd_animation.GetRotationsAttr();
			UsdAttribute scales_attr = usd_animation.GetScalesAttr();
			UsdAttribute blend_shape_weights_attr = usd_animation.GetBlendShapeWeightsAttr();

			std::unordered_map<std::string, int> joint_index_by_bone_name;
			for (int joint_index = 0; joint_index < joint_paths.size(); joint_index++) {
				const String joint_path = joint_paths[joint_index];
				const String bone_name = joint_leaf_name_for_save(joint_path);
				if (!bone_name.is_empty()) {
					joint_index_by_bone_name[bone_name.utf8().get_data()] = joint_index;
				}
			}

			const double time_codes_per_second = MAX((double)metadata.get("usd:time_codes_per_second", p_stage->GetTimeCodesPerSecond()), 1.0);
			const double start_time = (double)metadata.get("usd:start_time_code", 0.0);
			bool has_derived_inbetween_blend_shape_tracks = false;
			for (int track_index = 0; track_index < animation->get_track_count(); track_index++) {
				if (animation->track_get_type(track_index) != Animation::TYPE_BLEND_SHAPE) {
					continue;
				}
				const String blend_shape_name = node_path_property_name(animation->track_get_path(track_index));
				if (blend_shape_name.contains("__inbetween__")) {
					has_derived_inbetween_blend_shape_tracks = true;
					append_static_save_warning(r_warnings, vformat("derived inbetween blend-shape track at %s:%s is unsupported; source-aware static save currently only merges direct primary blend-shape weights", animation_prim_path, blend_shape_name));
				}
			}
			for (int track_index = 0; track_index < animation->get_track_count(); track_index++) {
				if (animation->track_get_type(track_index) == Animation::TYPE_BLEND_SHAPE) {
					if (has_derived_inbetween_blend_shape_tracks) {
						continue;
					}
					const Array blend_shape_names = metadata.get("usd:blend_shape_names", Array());
					const String blend_shape_name = node_path_property_name(animation->track_get_path(track_index));
					int blend_shape_index = -1;
					for (int i = 0; i < blend_shape_names.size(); i++) {
						if ((String)blend_shape_names[i] == blend_shape_name) {
							blend_shape_index = i;
							break;
						}
					}
					if (blend_shape_index < 0 || !blend_shape_weights_attr) {
						continue;
					}

					for (int key_index = 0; key_index < animation->track_get_key_count(track_index); key_index++) {
						const Variant value = animation->track_get_key_value(track_index, key_index);
						if (value.get_type() != Variant::FLOAT && value.get_type() != Variant::INT) {
							continue;
						}
						const double sample_time = start_time + animation->track_get_key_time(track_index, key_index) * time_codes_per_second;
						VtArray<float> blend_shape_weights;
						if (!blend_shape_weights_attr.Get(&blend_shape_weights, sample_time) || blend_shape_weights.size() != (size_t)blend_shape_names.size()) {
							continue;
						}
						const float edited_weight = (float)value;
						if (!Math::is_equal_approx((real_t)blend_shape_weights[blend_shape_index], (real_t)edited_weight)) {
							blend_shape_weights[blend_shape_index] = edited_weight;
							blend_shape_weights_attr.Set(blend_shape_weights, sample_time);
							applied_any = true;
						}
					}
					continue;
				}

				const String bone_name = node_path_bone_name(animation->track_get_path(track_index));
				const auto joint_it = joint_index_by_bone_name.find(bone_name.utf8().get_data());
				if (joint_it == joint_index_by_bone_name.end()) {
					continue;
				}
				const int joint_index = joint_it->second;

				for (int key_index = 0; key_index < animation->track_get_key_count(track_index); key_index++) {
					const Variant value = animation->track_get_key_value(track_index, key_index);
					const double sample_time = start_time + animation->track_get_key_time(track_index, key_index) * time_codes_per_second;

					if (animation->track_get_type(track_index) == Animation::TYPE_POSITION_3D && translations_attr && value.get_type() == Variant::VECTOR3) {
						VtArray<GfVec3f> translations;
						if (!translations_attr.Get(&translations, sample_time) || translations.size() != (size_t)joint_paths.size()) {
							continue;
						}
						const GfVec3f edited_translation = godot_vector_to_gf_vec3f((Vector3)value);
						if (!gf_vec3f_is_equal_approx(translations[joint_index], edited_translation)) {
							translations[joint_index] = edited_translation;
							translations_attr.Set(translations, sample_time);
							applied_any = true;
						}
					} else if (animation->track_get_type(track_index) == Animation::TYPE_ROTATION_3D && rotations_attr && value.get_type() == Variant::QUATERNION) {
						VtArray<GfQuatf> rotations;
						if (!rotations_attr.Get(&rotations, sample_time) || rotations.size() != (size_t)joint_paths.size()) {
							continue;
						}
						const GfQuatf edited_rotation = godot_quat_to_gf_quat((Quaternion)value);
						if (!gf_quat_is_equal_approx(rotations[joint_index], edited_rotation)) {
							rotations[joint_index] = edited_rotation;
							rotations_attr.Set(rotations, sample_time);
							applied_any = true;
						}
					} else if (animation->track_get_type(track_index) == Animation::TYPE_SCALE_3D && scales_attr && value.get_type() == Variant::VECTOR3) {
						VtArray<GfVec3h> scales;
						if (!scales_attr.Get(&scales, sample_time) || scales.size() != (size_t)joint_paths.size()) {
							continue;
						}
						const GfVec3h edited_scale = godot_vector_to_gf_vec3h((Vector3)value);
						if (!gf_vec3h_is_equal_approx(scales[joint_index], edited_scale)) {
							scales[joint_index] = edited_scale;
							scales_attr.Set(scales, sample_time);
							applied_any = true;
						}
					}
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(false); i++) {
		applied_any = apply_animation_edits_to_stage(p_node->get_child(i, false), p_stage, r_warnings) || applied_any;
	}

	return applied_any;
}
// ---- composition-preserving export (scene_loader:214-264,374-378,403-421,1281-1641) --------
Dictionary serialize_layer_offset_dict(const SdfLayerOffset &p_layer_offset) {
	Dictionary description;
	description["offset"] = p_layer_offset.GetOffset();
	description["scale"] = p_layer_offset.GetScale();
	return description;
}

void deserialize_layer_offset_dict(const Dictionary &p_description, SdfLayerOffset *r_layer_offset) {
	ERR_FAIL_NULL(r_layer_offset);
	const double offset = p_description.get("offset", 0.0);
	const double scale = p_description.get("scale", 1.0);
	*r_layer_offset = SdfLayerOffset(offset, scale);
}

bool deserialize_reference_dict(const Dictionary &p_description, SdfReference *r_reference) {
	ERR_FAIL_NULL_V(r_reference, false);
	const String asset_path = p_description.get("asset_path", String());
	const String prim_path = p_description.get("prim_path", String());
	SdfLayerOffset layer_offset;
	deserialize_layer_offset_dict(p_description.get("layer_offset", Dictionary()), &layer_offset);
	*r_reference = SdfReference(asset_path.utf8().get_data(), prim_path.is_empty() ? SdfPath() : SdfPath(prim_path.utf8().get_data()), layer_offset);
	return true;
}

bool deserialize_payload_dict(const Dictionary &p_description, SdfPayload *r_payload) {
	ERR_FAIL_NULL_V(r_payload, false);
	const String asset_path = p_description.get("asset_path", String());
	const String prim_path = p_description.get("prim_path", String());
	SdfLayerOffset layer_offset;
	deserialize_layer_offset_dict(p_description.get("layer_offset", Dictionary()), &layer_offset);
	*r_payload = SdfPayload(asset_path.utf8().get_data(), prim_path.is_empty() ? SdfPath() : SdfPath(prim_path.utf8().get_data()), layer_offset);
	return true;
}

SdfPathVector deserialize_path_array(const Array &p_paths) {
	SdfPathVector paths;
	for (int i = 0; i < p_paths.size(); i++) {
		if (p_paths[i].get_type() != Variant::STRING) {
			continue;
		}
		const String path = p_paths[i];
		if (path.is_empty()) {
			continue;
		}
		const SdfPath sdf_path(path.utf8().get_data());
		if (!sdf_path.IsEmpty()) {
			paths.push_back(sdf_path);
		}
	}
	return paths;
}

bool metadata_has_composition_arcs(const Dictionary &p_metadata) {
	return !((Array)p_metadata.get("usd:references", Array())).is_empty() ||
			!((Array)p_metadata.get("usd:payloads", Array())).is_empty() ||
			!((Array)p_metadata.get("usd:inherits", Array())).is_empty() ||
			!((Array)p_metadata.get("usd:specializes", Array())).is_empty();
}

bool usd_metadata_has_preserved_composition_boundary(const Dictionary &p_metadata) {
	return (bool)p_metadata.get("usd:variant_boundary", false) ||
			!((Array)p_metadata.get("usd:variant_context", Array())).is_empty() ||
			metadata_has_composition_arcs(p_metadata);
}

bool node_tree_has_usd_composition_boundaries(Node *p_node) {
	ERR_FAIL_NULL_V(p_node, false);
	// (No UsdStageInstance in this build — variant instances aren't ported yet; baked scenes only.)

	const Dictionary metadata = get_usd_metadata(p_node);
	if (usd_metadata_has_preserved_composition_boundary(metadata)) {
		return true;
	}

	for (int i = 0; i < p_node->get_child_count(false); i++) {
		if (node_tree_has_usd_composition_boundaries(p_node->get_child(i, false))) {
			return true;
		}
	}

	return false;
}

void reapply_composition_arcs(const UsdPrim &p_prim, const Object *p_source_object) {
	ERR_FAIL_NULL(p_source_object);
	if (!p_prim) {
		return;
	}

	const Dictionary metadata = get_usd_metadata(p_source_object);

	const Array references = metadata.get("usd:references", Array());
	if (!references.is_empty()) {
		SdfReferenceVector reference_items;
		for (int i = 0; i < references.size(); i++) {
			if (references[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			SdfReference reference;
			if (deserialize_reference_dict(references[i], &reference)) {
				reference_items.push_back(reference);
			}
		}
		if (!reference_items.empty()) {
			UsdReferences usd_references = p_prim.GetReferences();
			if (usd_references) {
				usd_references.SetReferences(reference_items);
			}
		}
	}

	const Array payloads = metadata.get("usd:payloads", Array());
	if (!payloads.is_empty()) {
		SdfPayloadVector payload_items;
		for (int i = 0; i < payloads.size(); i++) {
			if (payloads[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			SdfPayload payload;
			if (deserialize_payload_dict(payloads[i], &payload)) {
				payload_items.push_back(payload);
			}
		}
		if (!payload_items.empty()) {
			UsdPayloads usd_payloads = p_prim.GetPayloads();
			if (usd_payloads) {
				usd_payloads.SetPayloads(payload_items);
			}
		}
	}

	const Array inherits = metadata.get("usd:inherits", Array());
	if (!inherits.is_empty()) {
		const SdfPathVector inherit_items = deserialize_path_array(inherits);
		if (!inherit_items.empty()) {
			UsdInherits usd_inherits = p_prim.GetInherits();
			if (usd_inherits) {
				usd_inherits.SetInherits(inherit_items);
			}
		}
	}

	const Array specializes = metadata.get("usd:specializes", Array());
	if (!specializes.is_empty()) {
		const SdfPathVector specialize_items = deserialize_path_array(specializes);
		if (!specialize_items.empty()) {
			UsdSpecializes usd_specializes = p_prim.GetSpecializes();
			if (usd_specializes) {
				usd_specializes.SetSpecializes(specialize_items);
			}
		}
	}
}

void apply_source_prim_transform(const UsdPrim &p_source_prim, UsdPrim p_target_prim) {
	if (!p_source_prim || !p_target_prim) {
		return;
	}

	UsdGeomXformable source_xformable(p_source_prim);
	UsdGeomXformable target_xformable(p_target_prim);
	if (!source_xformable || !target_xformable) {
		return;
	}

	GfMatrix4d local_matrix(1.0);
	bool resets_xform_stack = false;
	if (!source_xformable.GetLocalTransformation(&local_matrix, &resets_xform_stack, UsdTimeCode::Default())) {
		return;
	}

	target_xformable.ClearXformOpOrder();
	target_xformable.AddTransformOp(UsdGeomXformOp::PrecisionDouble).Set(local_matrix);
}

bool transforms_equal_approx(const Transform3D &p_left, const Transform3D &p_right) {
	if (!p_left.origin.is_equal_approx(p_right.origin)) {
		return false;
	}
	for (int i = 0; i < 3; i++) {
		if (!p_left.basis.get_column(i).is_equal_approx(p_right.basis.get_column(i))) {
			return false;
		}
	}
	return true;
}

bool generated_node_state_matches(Node *p_current, Node *p_expected, String *r_reason) {
	ERR_FAIL_NULL_V(p_current, false);
	ERR_FAIL_NULL_V(p_expected, false);

	if (p_current->get_name() != p_expected->get_name()) {
		if (r_reason != nullptr) {
			*r_reason = vformat("name changed from '%s' to '%s'", p_expected->get_name(), p_current->get_name());
		}
		return false;
	}

	if (String(p_current->get_class()) != String(p_expected->get_class())) {
		if (r_reason != nullptr) {
			*r_reason = vformat("node type changed from '%s' to '%s'", p_expected->get_class(), p_current->get_class());
		}
		return false;
	}

	Node3D *current_3d = Object::cast_to<Node3D>(p_current);
	Node3D *expected_3d = Object::cast_to<Node3D>(p_expected);
	if ((current_3d == nullptr) != (expected_3d == nullptr)) {
		if (r_reason != nullptr) {
			*r_reason = "Node3D mapping changed";
		}
		return false;
	}
	if (current_3d != nullptr && expected_3d != nullptr) {
		if (!transforms_equal_approx(current_3d->get_transform(), expected_3d->get_transform())) {
			if (r_reason != nullptr) {
				*r_reason = "transform changed";
			}
			return false;
		}
		if (current_3d->is_visible() != expected_3d->is_visible()) {
			if (r_reason != nullptr) {
				*r_reason = "visibility changed";
			}
			return false;
		}
	}

	MeshInstance3D *current_mesh = Object::cast_to<MeshInstance3D>(p_current);
	MeshInstance3D *expected_mesh = Object::cast_to<MeshInstance3D>(p_expected);
	if ((current_mesh == nullptr) != (expected_mesh == nullptr)) {
		if (r_reason != nullptr) {
			*r_reason = "mesh node mapping changed";
		}
		return false;
	}
	if (current_mesh != nullptr && expected_mesh != nullptr) {
		const Ref<Mesh> current_mesh_resource = current_mesh->get_mesh();
		const Ref<Mesh> expected_mesh_resource = expected_mesh->get_mesh();
		const int current_surface_count = current_mesh_resource.is_valid() ? current_mesh_resource->get_surface_count() : 0;
		const int expected_surface_count = expected_mesh_resource.is_valid() ? expected_mesh_resource->get_surface_count() : 0;
		if (current_surface_count != expected_surface_count) {
			if (r_reason != nullptr) {
				*r_reason = "mesh surface count changed";
			}
			return false;
		}
	}

	return true;
}

void collect_generated_composition_boundary_nodes(Node *p_node, bool p_under_composition_boundary, std::unordered_map<std::string, Node *> *r_nodes, PackedStringArray *r_unmapped_nodes, int p_unmapped_limit = 12) {
	ERR_FAIL_NULL(p_node);
	ERR_FAIL_NULL(r_nodes);

	const Dictionary metadata = get_usd_metadata(p_node);
	if ((bool)metadata.get("usd:generated_preview", false)) {
		return;
	}

	const bool under_composition_boundary = p_under_composition_boundary || usd_metadata_has_preserved_composition_boundary(metadata);
	const String prim_path = metadata.get("usd:prim_path", String());

	if (under_composition_boundary) {
		if (!prim_path.is_empty()) {
			(*r_nodes)[std::string(prim_path.utf8().get_data())] = p_node;
		} else if (r_unmapped_nodes != nullptr && r_unmapped_nodes->size() < p_unmapped_limit) {
			r_unmapped_nodes->push_back(String(p_node->get_path()));
		}
	}

	for (int i = 0; i < p_node->get_child_count(false); i++) {
		collect_generated_composition_boundary_nodes(p_node->get_child(i, false), under_composition_boundary, r_nodes, r_unmapped_nodes, p_unmapped_limit);
	}
}


bool export_composition_preserving_node(Node *p_node, const UsdStageRefPtr &p_stage, const UsdStageRefPtr &p_source_stage) {
	ERR_FAIL_NULL_V(p_node, false);
	ERR_FAIL_COND_V(p_stage == nullptr, false);

	if (p_node->has_meta(StringName("usd")) && (bool)get_usd_metadata(p_node).get("usd:generated_preview", false)) {
		return true;
	}

	const Dictionary metadata = get_usd_metadata(p_node);
	const String prim_path_string = metadata.get("usd:prim_path", String());
	if (prim_path_string.is_empty()) {
		for (int i = 0; i < p_node->get_child_count(false); i++) {
			if (!export_composition_preserving_node(p_node->get_child(i, false), p_stage, p_source_stage)) {
				return false;
			}
		}
		return true;
	}

	const SdfPath prim_path(prim_path_string.utf8().get_data());
	if (prim_path.IsEmpty()) {
		return false;
	}

	const bool has_references = !((Array)metadata.get("usd:references", Array())).is_empty();
	const bool has_payloads = !((Array)metadata.get("usd:payloads", Array())).is_empty();
	const bool has_inherits = !((Array)metadata.get("usd:inherits", Array())).is_empty();
	const bool has_specializes = !((Array)metadata.get("usd:specializes", Array())).is_empty();

	UsdPrim prim;
	if (has_references || has_payloads) {
		prim = p_stage->OverridePrim(prim_path);
	} else if (Object::cast_to<Node3D>(p_node) != nullptr) {
		prim = UsdGeomXform::Define(p_stage, prim_path).GetPrim();
	} else {
		prim = p_stage->DefinePrim(prim_path);
	}
	if (!prim) {
		return false;
	}

	const UsdPrim source_prim = p_source_stage != nullptr ? p_source_stage->GetPrimAtPath(prim_path) : UsdPrim();
	if (source_prim) {
		apply_source_prim_transform(source_prim, prim);
	} else if (Node3D *node_3d = Object::cast_to<Node3D>(p_node)) {
		UsdGeomXformable xformable(prim);
		apply_node3d_transform(node_3d, xformable);
	}

	if (has_references || has_payloads || has_inherits || has_specializes) {
		reapply_composition_arcs(prim, p_node);
	}

	if (has_references || has_payloads) {
		return true;
	}

	for (int i = 0; i < p_node->get_child_count(false); i++) {
		if (!export_composition_preserving_node(p_node->get_child(i, false), p_stage, p_source_stage)) {
			return false;
		}
	}
	return true;
}

Error save_composition_preserving_generated_scene(Node *p_generated_root, const String &p_path, const UsdStageRefPtr &p_source_stage) {
	ERR_FAIL_NULL_V(p_generated_root, ERR_INVALID_PARAMETER);

	const String absolute_path = get_absolute_path(p_path);
	const String base_dir = absolute_path.get_base_dir();
	if (!base_dir.is_empty()) {
		const Error mkdir_error = DirAccess::make_dir_recursive_absolute(base_dir);
		if (mkdir_error != OK) {
			return mkdir_error;
		}
	}

	UsdStageRefPtr stage = UsdStage::CreateNew(absolute_path.utf8().get_data());
	if (!stage) {
		return ERR_CANT_CREATE;
	}

	UsdGeomSetStageUpAxis(stage, UsdGeomTokens->y);
	UsdGeomSetStageMetersPerUnit(stage, 1.0);

	for (int i = 0; i < p_generated_root->get_child_count(false); i++) {
		if (!export_composition_preserving_node(p_generated_root->get_child(i, false), stage, p_source_stage)) {
			return ERR_CANT_CREATE;
		}
	}

	if (p_generated_root->get_child_count(false) > 0) {
		Node *first_child = p_generated_root->get_child(0, false);
		const Dictionary first_metadata = get_usd_metadata(first_child);
		const String default_prim_path = first_metadata.get("usd:prim_path", String());
		if (!default_prim_path.is_empty()) {
			UsdPrim default_prim = stage->GetPrimAtPath(SdfPath(default_prim_path.utf8().get_data()));
			if (default_prim) {
				stage->SetDefaultPrim(default_prim);
			}
		}
	}

	const bool saved = stage->GetRootLayer()->Save();
	if (!saved || p_source_stage == nullptr || !p_source_stage->GetRootLayer()) {
		return saved ? OK : ERR_CANT_CREATE;
	}

	const String source_stage_path = to_godot_string(p_source_stage->GetRootLayer()->GetRealPath());
	const String target_base_dir = absolute_path.get_base_dir();
	const auto copy_relative_asset = [&](const String &p_asset_path) {
		if (p_asset_path.is_empty() || p_asset_path.is_absolute_path()) {
			return;
		}
		const String source_asset_path = get_absolute_path(source_stage_path.get_base_dir().path_join(p_asset_path));
		if (!FileAccess::file_exists(source_asset_path)) {
			return;
		}

		const String target_asset_path = get_absolute_path(target_base_dir.path_join(p_asset_path));
		const String target_asset_dir = target_asset_path.get_base_dir();
		if (!target_asset_dir.is_empty()) {
			const Error mkdir_error = DirAccess::make_dir_recursive_absolute(target_asset_dir);
			if (mkdir_error != OK) {
				return;
			}
		}

		Ref<FileAccess> source_file = FileAccess::open(source_asset_path, FileAccess::READ);
		if (source_file.is_null()) {
			return;
		}
		Ref<FileAccess> target_file = FileAccess::open(target_asset_path, FileAccess::WRITE);
		if (target_file.is_null()) {
			return;
		}
		target_file->store_buffer(source_file->get_buffer(source_file->get_length()));
	};

	std::function<void(Node *)> copy_relative_assets_recursive = [&](Node *p_node) {
		if (p_node == nullptr) {
			return;
		}
		const Dictionary metadata = get_usd_metadata(p_node);
		const Array references = metadata.get("usd:references", Array());
		for (int i = 0; i < references.size(); i++) {
			if (references[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			copy_relative_asset(((Dictionary)references[i]).get("asset_path", String()));
		}
		const Array payloads = metadata.get("usd:payloads", Array());
		for (int i = 0; i < payloads.size(); i++) {
			if (payloads[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			copy_relative_asset(((Dictionary)payloads[i]).get("asset_path", String()));
		}
		for (int i = 0; i < p_node->get_child_count(false); i++) {
			copy_relative_assets_recursive(p_node->get_child(i, false));
		}
	};
	copy_relative_assets_recursive(p_generated_root);

	return OK;
}
} // anonymous namespace

// ---- orchestrator (scene_loader:1247) ------------------------------------------------------
Error save_static_imported_data_to_source_copy(Node *p_root, const String &p_source_absolute_path, const String &p_destination_absolute_path) {
	ERR_FAIL_NULL_V(p_root, ERR_INVALID_PARAMETER);

	const Error copy_error = copy_file_absolute_preserving_contents(p_source_absolute_path, p_destination_absolute_path);
	ERR_FAIL_COND_V_MSG(copy_error != OK, copy_error, vformat("Failed to copy source USD layer for source-aware static save: %s", p_source_absolute_path));

	SdfLayerRefPtr root_layer = SdfLayer::FindOrOpen(p_destination_absolute_path.utf8().get_data());
	ERR_FAIL_COND_V_MSG(!root_layer, ERR_CANT_OPEN, vformat("Failed to open copied USD layer for source-aware static save: %s", p_destination_absolute_path));

	UsdStageRefPtr stage = UsdStage::Open(root_layer);
	ERR_FAIL_COND_V_MSG(!stage, ERR_CANT_OPEN, vformat("Failed to compose copied USD layer for source-aware static save: %s", p_destination_absolute_path));
	UsdStageRefPtr source_stage = UsdStage::Open(p_source_absolute_path.utf8().get_data());
	if (!source_stage) {
		source_stage = stage;
	}

	stage->SetEditTarget(root_layer);
	PackedStringArray unsupported_warnings;
	warn_unsupported_static_mesh_edits(p_root, source_stage, &unsupported_warnings);
	const bool applied_transform_edits = apply_node3d_transform_edits_to_stage(p_root, stage);
	const bool applied_material_edits = apply_material_edits_to_stage(p_root, stage, &unsupported_warnings);
	const bool applied_mesh_point_edits = apply_mesh_point_edits_to_stage(p_root, stage, &unsupported_warnings);
	const bool applied_skeleton_edits = apply_skeleton_rest_edits_to_stage(p_root, stage);
	const bool applied_animation_edits = apply_animation_edits_to_stage(p_root, stage, &unsupported_warnings);
	for (int i = 0; i < unsupported_warnings.size(); i++) {
		UtilityFunctions::push_warning(vformat("USD source-aware static save could not merge unsupported edit: %s", unsupported_warnings[i]));
	}
	if (!applied_transform_edits && !applied_material_edits && !applied_mesh_point_edits && !applied_skeleton_edits && !applied_animation_edits) {
		return OK;
	}

	return root_layer->Save() ? OK : ERR_CANT_CREATE;
}

// Dispatch: a baked scene with preserved read-only composition boundaries is re-exported as
// override prims that reauthor the arcs (over "Car" (references=...)); otherwise the source layer
// is copied and supported static edits merged in.
Error save_source_preserving_layer(Node *p_root, const String &p_source_absolute_path, const String &p_destination_absolute_path) {
	if (node_tree_has_usd_composition_boundaries(p_root)) {
		UsdStageRefPtr source_stage = UsdStage::Open(p_source_absolute_path.utf8().get_data());
		return save_composition_preserving_generated_scene(p_root, p_destination_absolute_path, source_stage);
	}
	return save_static_imported_data_to_source_copy(p_root, p_source_absolute_path, p_destination_absolute_path);
}
} // namespace godot_usd
