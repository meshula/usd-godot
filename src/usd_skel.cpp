// ---------------------------------------------------------------------------------------------
// Transplanted from godot-usd (https://github.com/migueldeicaza) — MIT License, (c) 2026 Miguel de
// Icaza. Harvested into usd-godot per USD_GODOT_INTEGRATION_PLAN.md (Phase 3), lifted near-verbatim.
// NOTE: build_skeleton_node currently resolves joint hierarchy by joint-path string parsing; a
// follow-up rewrites that on official UsdSkelSkeletonQuery/Topology (tracked in the plan).
// See THIRD_PARTY.md for provenance.
// ---------------------------------------------------------------------------------------------

#include "usd_skel.h"

#include "usd_stage_utils.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <godot_cpp/classes/animation.hpp>
#include <godot_cpp/classes/animation_library.hpp>
#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/skin.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/math.hpp>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/quatf.h>
#include <pxr/base/gf/vec3h.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/relationship.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/skeleton.h>

namespace godot_usd {

namespace {

String joint_leaf_name(const String &p_joint_path) {
	const int separator = p_joint_path.rfind("/");
	if (separator < 0) {
		return p_joint_path;
	}
	return p_joint_path.substr(separator + 1);
}

String joint_parent_path(const String &p_joint_path) {
	const int separator = p_joint_path.rfind("/");
	if (separator < 0) {
		return String();
	}
	return p_joint_path.substr(0, separator);
}

Array to_token_array(const VtArray<TfToken> &p_tokens) {
	Array result;
	for (const TfToken &token : p_tokens) {
		result.push_back(to_godot_string(token.GetString()));
	}
	return result;
}

Quaternion gf_quat_to_godot(const GfQuatf &p_quat) {
	const GfVec3f imaginary = p_quat.GetImaginary();
	return Quaternion(imaginary[0], imaginary[1], imaginary[2], p_quat.GetReal());
}

Node *find_node_for_prim_path(Node *p_node, const String &p_prim_path) {
	ERR_FAIL_NULL_V(p_node, nullptr);
	const Dictionary metadata = get_usd_metadata(p_node);
	if ((String)metadata.get("usd:prim_path", String()) == p_prim_path) {
		return p_node;
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		if (Node *found = find_node_for_prim_path(p_node->get_child(i), p_prim_path)) {
			return found;
		}
	}
	return nullptr;
}

String node_name_for_prim(const UsdPrim &p_prim) {
	return to_godot_string(p_prim.GetName().GetString());
}

String make_unique_animation_name(const Ref<AnimationLibrary> &p_library, const String &p_animation_name) {
	if (p_library.is_null() || !p_library->has_animation(StringName(p_animation_name))) {
		return p_animation_name;
	}

	for (int suffix = 1;; suffix++) {
		const String candidate = vformat("%s_%d", p_animation_name, suffix);
		if (!p_library->has_animation(StringName(candidate))) {
			return candidate;
		}
	}
}

Array sample_times_to_array(const std::vector<double> &p_times) {
	Array result;
	for (double sample_time : p_times) {
		result.push_back(sample_time);
	}
	return result;
}

String make_inbetween_blend_shape_channel_name(const String &p_blend_shape_name, const String &p_inbetween_name) {
	return p_blend_shape_name + String("__inbetween__") + p_inbetween_name;
}

struct BlendShapeChannelSpec {
	String channel_name;
	float weight = 1.0f;
	bool primary = false;
};

struct BlendShapeChannelWeightComparator {
	bool operator()(const BlendShapeChannelSpec &p_left, const BlendShapeChannelSpec &p_right) const {
		if (Math::is_equal_approx(p_left.weight, p_right.weight)) {
			if (p_left.primary != p_right.primary) {
				return !p_left.primary && p_right.primary;
			}
			return p_left.channel_name < p_right.channel_name;
		}
		return p_left.weight < p_right.weight;
	}
};

std::vector<BlendShapeChannelSpec> parse_blend_shape_channel_specs(const Variant &p_channel_metadata) {
	std::vector<BlendShapeChannelSpec> specs;
	if (p_channel_metadata.get_type() != Variant::ARRAY) {
		return specs;
	}

	const Array channel_array = p_channel_metadata;
	for (int i = 0; i < channel_array.size(); i++) {
		if (channel_array[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary channel_dict = channel_array[i];
		const String channel_name = channel_dict.get("channel_name", String());
		if (channel_name.is_empty()) {
			continue;
		}

		BlendShapeChannelSpec spec;
		spec.channel_name = channel_name;
		spec.weight = (float)(double)channel_dict.get("weight", 1.0);
		spec.primary = (bool)channel_dict.get("primary", false);
		spec.weight = MAX(spec.weight, 0.0001f);
		specs.push_back(spec);
	}

	std::sort(specs.begin(), specs.end(), BlendShapeChannelWeightComparator());
	return specs;
}

std::unordered_map<std::string, float> evaluate_blend_shape_channel_weights(float p_source_weight, const std::vector<BlendShapeChannelSpec> &p_specs) {
	std::unordered_map<std::string, float> weights_by_channel;
	if (p_specs.empty() || p_source_weight <= 0.0f) {
		return weights_by_channel;
	}

	if (p_specs.size() == 1) {
		weights_by_channel[p_specs[0].channel_name.utf8().get_data()] = p_source_weight / p_specs[0].weight;
		return weights_by_channel;
	}

	if (p_source_weight < p_specs[0].weight) {
		weights_by_channel[p_specs[0].channel_name.utf8().get_data()] = p_source_weight / p_specs[0].weight;
		return weights_by_channel;
	}

	for (int spec_index = 0; spec_index < (int)p_specs.size() - 1; spec_index++) {
		const BlendShapeChannelSpec &current = p_specs[spec_index];
		const BlendShapeChannelSpec &next = p_specs[spec_index + 1];
		if (p_source_weight <= next.weight) {
			const float span = MAX(next.weight - current.weight, 0.0001f);
			const float alpha = CLAMP((p_source_weight - current.weight) / span, 0.0f, 1.0f);
			weights_by_channel[current.channel_name.utf8().get_data()] = 1.0f - alpha;
			weights_by_channel[next.channel_name.utf8().get_data()] = alpha;
			return weights_by_channel;
		}
	}

	weights_by_channel[p_specs.back().channel_name.utf8().get_data()] = p_source_weight / p_specs.back().weight;
	return weights_by_channel;
}

std::vector<double> build_blend_shape_sample_times(const UsdAttribute &p_blend_shape_weights_attr, int p_animation_blend_shape_index, const std::vector<BlendShapeChannelSpec> &p_specs, const std::vector<double> &p_base_sample_times) {
	std::vector<double> sample_times = p_base_sample_times;
	if (!p_blend_shape_weights_attr || p_specs.size() <= 1 || p_base_sample_times.size() <= 1) {
		return sample_times;
	}

	for (int sample_index = 0; sample_index < (int)p_base_sample_times.size() - 1; sample_index++) {
		const double start_sample_time = p_base_sample_times[sample_index];
		const double end_sample_time = p_base_sample_times[sample_index + 1];

		VtArray<float> start_weights;
		VtArray<float> end_weights;
		if (!p_blend_shape_weights_attr.Get(&start_weights, start_sample_time) || !p_blend_shape_weights_attr.Get(&end_weights, end_sample_time)) {
			continue;
		}
		if (p_animation_blend_shape_index >= (int)start_weights.size() || p_animation_blend_shape_index >= (int)end_weights.size()) {
			continue;
		}

		const float start_weight = start_weights[p_animation_blend_shape_index];
		const float end_weight = end_weights[p_animation_blend_shape_index];
		if (Math::is_equal_approx(start_weight, end_weight)) {
			continue;
		}

		const float low_weight = MIN(start_weight, end_weight);
		const float high_weight = MAX(start_weight, end_weight);
		for (int spec_index = 0; spec_index < (int)p_specs.size() - 1; spec_index++) {
			const float threshold = p_specs[spec_index].weight;
			if (threshold <= low_weight || threshold >= high_weight) {
				continue;
			}
			const double alpha = (threshold - start_weight) / (end_weight - start_weight);
			if (alpha <= 0.0 || alpha >= 1.0) {
				continue;
			}
			sample_times.push_back(start_sample_time + (end_sample_time - start_sample_time) * alpha);
		}
	}

	std::sort(sample_times.begin(), sample_times.end());
	sample_times.erase(std::unique(sample_times.begin(), sample_times.end(), [](double p_left, double p_right) {
							   return Math::is_equal_approx(p_left, p_right);
						   }),
			sample_times.end());
	return sample_times;
}

AnimationPlayer *get_or_create_animation_player(Node3D *p_root) {
	ERR_FAIL_NULL_V(p_root, nullptr);

	for (int i = 0; i < p_root->get_child_count(); i++) {
		if (AnimationPlayer *existing = Object::cast_to<AnimationPlayer>(p_root->get_child(i))) {
			return existing;
		}
	}

	String player_name = "USDAnimationPlayer";
	for (int suffix = 1; p_root->get_node_or_null(NodePath(player_name)) != nullptr; suffix++) {
		player_name = vformat("USDAnimationPlayer%d", suffix);
	}

	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_name(player_name);
	player->set_root_node(NodePath(".."));
	p_root->add_child(player);

	Ref<AnimationLibrary> library;
	library.instantiate();
	player->add_animation_library(StringName(), library);
	return player;
}

bool append_baked_skeleton_animation(Node3D *p_scene_root, Skeleton3D *p_skeleton, const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdSkelAnimation &p_animation) {
	ERR_FAIL_NULL_V(p_scene_root, false);
	ERR_FAIL_NULL_V(p_skeleton, false);
	ERR_FAIL_COND_V(!p_animation, false);

	VtArray<TfToken> animation_joints;
	p_animation.GetJointsAttr().Get(&animation_joints, p_time);

	std::unordered_map<std::string, int> bone_index_by_joint_path;
	for (int bone_index = 0; bone_index < p_skeleton->get_bone_count(); bone_index++) {
		if (!p_skeleton->has_bone_meta(bone_index, StringName("usd_joint_path"))) {
			continue;
		}
		const String joint_path = p_skeleton->get_bone_meta(bone_index, StringName("usd_joint_path"));
		bone_index_by_joint_path[joint_path.utf8().get_data()] = bone_index;
	}

	UsdAttribute translations_attr = p_animation.GetTranslationsAttr();
	UsdAttribute rotations_attr = p_animation.GetRotationsAttr();
	UsdAttribute scales_attr = p_animation.GetScalesAttr();
	UsdAttribute blend_shape_weights_attr = p_animation.GetBlendShapeWeightsAttr();
	VtArray<TfToken> animation_blend_shape_names;
	const bool has_animation_blend_shapes = p_animation.GetBlendShapesAttr().Get(&animation_blend_shape_names, p_time) && !animation_blend_shape_names.empty();

	const bool has_translations = translations_attr && translations_attr.HasAuthoredValueOpinion();
	const bool has_rotations = rotations_attr && rotations_attr.HasAuthoredValueOpinion();
	const bool has_scales = scales_attr && scales_attr.HasAuthoredValueOpinion();
	const bool has_blend_shape_weights = blend_shape_weights_attr && blend_shape_weights_attr.HasAuthoredValueOpinion() && has_animation_blend_shapes;
	if (!has_translations && !has_rotations && !has_scales && !has_blend_shape_weights) {
		return false;
	}

	std::vector<double> translation_sample_times;
	std::vector<double> sample_times;
	std::vector<double> rotation_sample_times;
	std::vector<double> scale_sample_times;
	std::vector<double> blend_shape_sample_times;
	auto append_time_samples = [&sample_times](const UsdAttribute &p_attr, std::vector<double> *r_attr_times) {
		if (!p_attr || !p_attr.HasAuthoredValueOpinion()) {
			return;
		}
		std::vector<double> attr_times;
		p_attr.GetTimeSamples(&attr_times);
		if (r_attr_times != nullptr) {
			*r_attr_times = attr_times;
		}
		sample_times.insert(sample_times.end(), attr_times.begin(), attr_times.end());
	};
	append_time_samples(translations_attr, &translation_sample_times);
	append_time_samples(rotations_attr, &rotation_sample_times);
	append_time_samples(scales_attr, &scale_sample_times);
	append_time_samples(blend_shape_weights_attr, &blend_shape_sample_times);
	if (sample_times.empty()) {
		sample_times.push_back(0.0);
	}
	std::sort(sample_times.begin(), sample_times.end());
	sample_times.erase(std::unique(sample_times.begin(), sample_times.end()), sample_times.end());

	const double time_codes_per_second = MAX(p_stage->GetTimeCodesPerSecond(), 1.0);
	const double start_time = sample_times.front();
	const double end_time = sample_times.back();

	AnimationPlayer *player = get_or_create_animation_player(p_scene_root);
	ERR_FAIL_NULL_V(player, false);
	Ref<AnimationLibrary> library = player->get_animation_library(StringName());
	if (library.is_null()) {
		library.instantiate();
		player->add_animation_library(StringName(), library);
	}

Ref<Animation> animation;
	animation.instantiate();
	animation->set_step(1.0 / time_codes_per_second);
	animation->set_length(MAX((end_time - start_time) / time_codes_per_second, 0.0));

	const String animation_name = make_unique_animation_name(library, node_name_for_prim(p_animation.GetPrim()));
	animation->set_name(animation_name);
	Dictionary animation_metadata;
	animation_metadata["usd:animation_prim_path"] = to_godot_string(p_animation.GetPrim().GetPath().GetString());
	animation_metadata["usd:time_codes_per_second"] = time_codes_per_second;
	animation_metadata["usd:start_time_code"] = start_time;
	animation_metadata["usd:end_time_code"] = end_time;
	animation_metadata["usd:has_authored_translations"] = has_translations;
	animation_metadata["usd:has_authored_rotations"] = has_rotations;
	animation_metadata["usd:has_authored_scales"] = has_scales;
	animation_metadata["usd:has_authored_blend_shape_weights"] = has_blend_shape_weights;
	animation_metadata["usd:translations_constant"] = has_translations && translation_sample_times.empty();
	animation_metadata["usd:rotations_constant"] = has_rotations && rotation_sample_times.empty();
	animation_metadata["usd:scales_constant"] = has_scales && scale_sample_times.empty();
	animation_metadata["usd:blend_shape_weights_constant"] = has_blend_shape_weights && blend_shape_sample_times.empty();
	animation_metadata["usd:joint_paths"] = to_token_array(animation_joints);
	if (has_animation_blend_shapes) {
		animation_metadata["usd:blend_shape_names"] = to_token_array(animation_blend_shape_names);
	}
	animation_metadata["usd:translation_time_codes"] = sample_times_to_array(translation_sample_times);
	animation_metadata["usd:rotation_time_codes"] = sample_times_to_array(rotation_sample_times);
	animation_metadata["usd:scale_time_codes"] = sample_times_to_array(scale_sample_times);
	animation_metadata["usd:blend_shape_weight_time_codes"] = sample_times_to_array(blend_shape_sample_times);

	VtArray<GfVec3f> default_translations;
	if (has_translations && translations_attr.Get(&default_translations, UsdTimeCode::Default()) && default_translations.size() == animation_joints.size()) {
		Array translation_defaults;
		for (int joint_index = 0; joint_index < (int)default_translations.size(); joint_index++) {
			const GfVec3f value = default_translations[joint_index];
			translation_defaults.push_back(Vector3(value[0], value[1], value[2]));
		}
		animation_metadata["usd:translation_defaults"] = translation_defaults;
	}

	VtArray<GfQuatf> default_rotations;
	if (has_rotations && rotations_attr.Get(&default_rotations, UsdTimeCode::Default()) && default_rotations.size() == animation_joints.size()) {
		Array rotation_defaults;
		for (int joint_index = 0; joint_index < (int)default_rotations.size(); joint_index++) {
			rotation_defaults.push_back(gf_quat_to_godot(default_rotations[joint_index]));
		}
		animation_metadata["usd:rotation_defaults"] = rotation_defaults;
	}

	VtArray<GfVec3h> default_scales;
	if (has_scales && scales_attr.Get(&default_scales, UsdTimeCode::Default()) && default_scales.size() == animation_joints.size()) {
		Array scale_defaults;
		for (int joint_index = 0; joint_index < (int)default_scales.size(); joint_index++) {
			const GfVec3h value = default_scales[joint_index];
			scale_defaults.push_back(Vector3((real_t)value[0], (real_t)value[1], (real_t)value[2]));
		}
		animation_metadata["usd:scale_defaults"] = scale_defaults;
	}

	VtArray<float> default_blend_shape_weights;
	if (has_blend_shape_weights && blend_shape_weights_attr.Get(&default_blend_shape_weights, UsdTimeCode::Default()) && default_blend_shape_weights.size() == animation_blend_shape_names.size()) {
		Array blend_shape_defaults;
		for (int blend_shape_index = 0; blend_shape_index < (int)default_blend_shape_weights.size(); blend_shape_index++) {
			blend_shape_defaults.push_back(default_blend_shape_weights[blend_shape_index]);
		}
		animation_metadata["usd:blend_shape_weight_defaults"] = blend_shape_defaults;
	}
	set_usd_metadata_entries(animation.ptr(), animation_metadata);

	const String skeleton_path = String(p_scene_root->get_path_to(p_skeleton));

	bool added_any_tracks = false;
	for (int joint_index = 0; joint_index < (int)animation_joints.size(); joint_index++) {
		const std::string joint_path_key = animation_joints[joint_index].GetString();
		const auto bone_it = bone_index_by_joint_path.find(joint_path_key);
		if (bone_it == bone_index_by_joint_path.end()) {
			continue;
		}

		const int bone_index = bone_it->second;
		const String track_path = skeleton_path + String(":") + p_skeleton->get_bone_name(bone_index);
		const Transform3D rest = p_skeleton->get_bone_rest(bone_index);
		const Vector3 rest_position = rest.origin;
		const Quaternion rest_rotation = p_skeleton->get_bone_rest(bone_index).basis.get_rotation_quaternion();
		const Vector3 rest_scale = rest.basis.get_scale();

		bool add_position_track = false;
		if (has_translations) {
			for (double sample_time : sample_times) {
				VtArray<GfVec3f> translations;
				if (translations_attr.Get(&translations, sample_time) && joint_index < (int)translations.size()) {
					const GfVec3f value = translations[joint_index];
					if (!Vector3(value[0], value[1], value[2]).is_equal_approx(rest_position)) {
						add_position_track = true;
						break;
					}
				}
			}
		}

		bool add_rotation_track = false;
		if (has_rotations) {
			for (double sample_time : sample_times) {
				VtArray<GfQuatf> rotations;
				if (rotations_attr.Get(&rotations, sample_time) && joint_index < (int)rotations.size()) {
					if (!gf_quat_to_godot(rotations[joint_index]).is_equal_approx(rest_rotation)) {
						add_rotation_track = true;
						break;
					}
				}
			}
		}

		bool add_scale_track = false;
		if (has_scales) {
			for (double sample_time : sample_times) {
				VtArray<GfVec3h> scales;
				if (scales_attr.Get(&scales, sample_time) && joint_index < (int)scales.size()) {
					const GfVec3h value = scales[joint_index];
					if (!Vector3((real_t)value[0], (real_t)value[1], (real_t)value[2]).is_equal_approx(rest_scale)) {
						add_scale_track = true;
						break;
					}
				}
			}
		}

		const int position_track = add_position_track ? animation->get_track_count() : -1;
		if (add_position_track) {
			animation->add_track(Animation::TYPE_POSITION_3D);
			animation->track_set_path(position_track, NodePath(track_path));
			animation->track_set_imported(position_track, true);
		}

		const int rotation_track = add_rotation_track ? animation->get_track_count() : -1;
		if (add_rotation_track) {
			animation->add_track(Animation::TYPE_ROTATION_3D);
			animation->track_set_path(rotation_track, NodePath(track_path));
			animation->track_set_imported(rotation_track, true);
		}

		const int scale_track = add_scale_track ? animation->get_track_count() : -1;
		if (add_scale_track) {
			animation->add_track(Animation::TYPE_SCALE_3D);
			animation->track_set_path(scale_track, NodePath(track_path));
			animation->track_set_imported(scale_track, true);
		}

		for (double sample_time : sample_times) {
			const double key_time = (sample_time - start_time) / time_codes_per_second;

			if (position_track >= 0) {
				VtArray<GfVec3f> translations;
				if (translations_attr.Get(&translations, sample_time) && joint_index < (int)translations.size()) {
					const GfVec3f value = translations[joint_index];
					animation->position_track_insert_key(position_track, key_time, Vector3(value[0], value[1], value[2]));
					added_any_tracks = true;
				}
			}

			if (rotation_track >= 0) {
				VtArray<GfQuatf> rotations;
				if (rotations_attr.Get(&rotations, sample_time) && joint_index < (int)rotations.size()) {
					animation->rotation_track_insert_key(rotation_track, key_time, gf_quat_to_godot(rotations[joint_index]));
					added_any_tracks = true;
				}
			}

			if (scale_track >= 0) {
				VtArray<GfVec3h> scales;
				if (scales_attr.Get(&scales, sample_time) && joint_index < (int)scales.size()) {
					const GfVec3h value = scales[joint_index];
					animation->scale_track_insert_key(scale_track, key_time, Vector3((real_t)value[0], (real_t)value[1], (real_t)value[2]));
					added_any_tracks = true;
				}
			}
		}
	}

	if (has_blend_shape_weights) {
		const String skeleton_prim_path = get_usd_metadata(p_skeleton).get("usd:prim_path", String());
		std::vector<Node *> stack;
		stack.push_back(p_scene_root);
		while (!stack.empty()) {
			Node *node = stack.back();
			stack.pop_back();

			for (int child_index = 0; child_index < node->get_child_count(); child_index++) {
				stack.push_back(node->get_child(child_index));
			}

			MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(node);
			Ref<ArrayMesh> array_mesh = mesh_instance != nullptr ? Ref<ArrayMesh>(mesh_instance->get_mesh()) : Ref<ArrayMesh>();
			if (mesh_instance == nullptr || array_mesh.is_null() || array_mesh->get_blend_shape_count() == 0) {
				continue;
			}

			const Dictionary mesh_metadata = get_usd_metadata(mesh_instance);
			if ((String)mesh_metadata.get("usd:skel_skeleton_path", String()) != skeleton_prim_path) {
				continue;
			}

			std::unordered_map<std::string, bool> mesh_blend_shape_names;
			for (int blend_shape_index = 0; blend_shape_index < array_mesh->get_blend_shape_count(); blend_shape_index++) {
				const String blend_shape_name = String(array_mesh->get_blend_shape_name(blend_shape_index));
				mesh_blend_shape_names[blend_shape_name.utf8().get_data()] = true;
			}

			const Dictionary mesh_blend_shape_channels = mesh_metadata.get("usd:blend_shape_channels", Dictionary());
			const String mesh_path = String(p_scene_root->get_path_to(mesh_instance));
			for (int animation_blend_shape_index = 0; animation_blend_shape_index < (int)animation_blend_shape_names.size(); animation_blend_shape_index++) {
				const String blend_shape_name = to_godot_string(animation_blend_shape_names[animation_blend_shape_index].GetString());
				std::vector<BlendShapeChannelSpec> channel_specs = parse_blend_shape_channel_specs(mesh_blend_shape_channels.get(blend_shape_name, Variant()));
				if (channel_specs.empty()) {
					BlendShapeChannelSpec fallback_spec;
					fallback_spec.channel_name = blend_shape_name;
					fallback_spec.weight = 1.0f;
					fallback_spec.primary = true;
					channel_specs.push_back(fallback_spec);
				}

				bool has_all_channels = true;
				for (const BlendShapeChannelSpec &channel_spec : channel_specs) {
					if (mesh_blend_shape_names.find(channel_spec.channel_name.utf8().get_data()) == mesh_blend_shape_names.end()) {
						has_all_channels = false;
						break;
					}
				}
				if (!has_all_channels) {
					continue;
				}

				const std::vector<double> channel_sample_times = build_blend_shape_sample_times(blend_shape_weights_attr, animation_blend_shape_index, channel_specs, sample_times);
				std::unordered_map<std::string, bool> add_blend_shape_track_by_channel;
				for (const BlendShapeChannelSpec &channel_spec : channel_specs) {
					add_blend_shape_track_by_channel[channel_spec.channel_name.utf8().get_data()] = false;
				}

				for (double sample_time : channel_sample_times) {
					VtArray<float> blend_shape_weights;
					if (blend_shape_weights_attr.Get(&blend_shape_weights, sample_time) && animation_blend_shape_index < (int)blend_shape_weights.size()) {
						const std::unordered_map<std::string, float> evaluated_weights = evaluate_blend_shape_channel_weights(blend_shape_weights[animation_blend_shape_index], channel_specs);
						for (const BlendShapeChannelSpec &channel_spec : channel_specs) {
							const auto weight_it = evaluated_weights.find(channel_spec.channel_name.utf8().get_data());
							if (weight_it != evaluated_weights.end() && !Math::is_equal_approx(weight_it->second, 0.0f)) {
								add_blend_shape_track_by_channel[channel_spec.channel_name.utf8().get_data()] = true;
							}
						}
					}
				}

				std::unordered_map<std::string, int> blend_shape_tracks_by_channel;
				for (const BlendShapeChannelSpec &channel_spec : channel_specs) {
					const auto add_it = add_blend_shape_track_by_channel.find(channel_spec.channel_name.utf8().get_data());
					if (add_it == add_blend_shape_track_by_channel.end() || !add_it->second) {
						continue;
					}
					const int blend_shape_track = animation->get_track_count();
					animation->add_track(Animation::TYPE_BLEND_SHAPE);
					animation->track_set_path(blend_shape_track, NodePath(mesh_path + String(":") + channel_spec.channel_name));
					animation->track_set_imported(blend_shape_track, true);
					blend_shape_tracks_by_channel[channel_spec.channel_name.utf8().get_data()] = blend_shape_track;
				}

				for (double sample_time : channel_sample_times) {
					VtArray<float> blend_shape_weights;
					if (!blend_shape_weights_attr.Get(&blend_shape_weights, sample_time) || animation_blend_shape_index >= (int)blend_shape_weights.size()) {
						continue;
					}
					const std::unordered_map<std::string, float> evaluated_weights = evaluate_blend_shape_channel_weights(blend_shape_weights[animation_blend_shape_index], channel_specs);
					for (const BlendShapeChannelSpec &channel_spec : channel_specs) {
						const auto track_it = blend_shape_tracks_by_channel.find(channel_spec.channel_name.utf8().get_data());
						if (track_it == blend_shape_tracks_by_channel.end()) {
							continue;
						}
						const auto weight_it = evaluated_weights.find(channel_spec.channel_name.utf8().get_data());
						const float channel_weight = weight_it != evaluated_weights.end() ? weight_it->second : 0.0f;
						animation->blend_shape_track_insert_key(track_it->second, (sample_time - start_time) / time_codes_per_second, channel_weight);
						added_any_tracks = true;
					}
				}
			}
		}
	}

	if (!added_any_tracks && !has_translations && !has_rotations && !has_scales && !has_blend_shape_weights) {
		return false;
	}

	library->add_animation(StringName(animation_name), animation);
	return true;
}

Ref<Skin> build_mesh_skin(Skeleton3D *p_skeleton, const Transform3D &p_geom_bind_transform) {
	ERR_FAIL_NULL_V(p_skeleton, Ref<Skin>());

	Ref<Skin> skin;
	skin.instantiate();
	skin->set_bind_count(p_skeleton->get_bone_count());

	for (int bone_index = 0; bone_index < p_skeleton->get_bone_count(); bone_index++) {
		Transform3D bind_transform = p_skeleton->get_bone_global_rest(bone_index);
		if (p_skeleton->has_bone_meta(bone_index, StringName("usd_joint_bind_transform"))) {
			bind_transform = p_skeleton->get_bone_meta(bone_index, StringName("usd_joint_bind_transform"));
		}

		skin->set_bind_bone(bone_index, bone_index);
		skin->set_bind_pose(bone_index, bind_transform.affine_inverse() * p_geom_bind_transform);
	}

	return skin;
}

} // namespace

Skeleton3D *build_skeleton_node(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const Transform3D &p_stage_correction_transform, const UsdPrim &p_prim, Dictionary *r_mapping_notes) {
	UsdSkelSkeleton usd_skeleton(p_prim);
	Skeleton3D *skeleton = memnew(Skeleton3D);

	VtArray<TfToken> joints;
	if (!usd_skeleton.GetJointsAttr().Get(&joints, p_time) || joints.empty()) {
		if (r_mapping_notes != nullptr) {
			(*r_mapping_notes)["usd:mapping_status"] = "Skeleton prim was detected, but its joints attribute could not be read.";
		}
		return skeleton;
	}

	VtArray<GfMatrix4d> rest_transforms;
	const bool has_rest_transforms = usd_skeleton.GetRestTransformsAttr().Get(&rest_transforms, p_time) && rest_transforms.size() == joints.size();

	VtArray<GfMatrix4d> bind_transforms;
	const bool has_bind_transforms = usd_skeleton.GetBindTransformsAttr().Get(&bind_transforms, p_time) && bind_transforms.size() == joints.size();

	set_usd_metadata(skeleton, "usd:skeleton_joint_count", (int)joints.size());
	set_usd_metadata(skeleton, "usd:skeleton_joint_paths", to_token_array(joints));
	set_usd_metadata(skeleton, "usd:skeleton_has_rest_transforms", has_rest_transforms);
	set_usd_metadata(skeleton, "usd:skeleton_has_bind_transforms", has_bind_transforms);
	set_usd_metadata(skeleton, "usd:skeleton_mapping", "skeleton3d_bones");

	UsdRelationship animation_source_rel = p_prim.GetRelationship(TfToken("skel:animationSource"));
	if (animation_source_rel) {
		SdfPathVector targets;
		animation_source_rel.GetTargets(&targets);
		if (!targets.empty()) {
			Array animation_sources;
			for (const SdfPath &target : targets) {
				animation_sources.push_back(to_godot_string(target.GetString()));
			}
			set_usd_metadata(skeleton, "usd:animation_sources", animation_sources);
		}
	}

	std::unordered_map<std::string, int> bone_index_by_joint_path;
	std::unordered_set<std::string> used_bone_names;

	for (int joint_index = 0; joint_index < (int)joints.size(); joint_index++) {
		const String joint_path = to_godot_string(joints[joint_index].GetString());
		String bone_name = joint_leaf_name(joint_path);
		if (bone_name.is_empty()) {
			bone_name = vformat("Bone%d", joint_index);
		}

		String unique_bone_name = bone_name;
		for (int suffix = 1; used_bone_names.count(std::string(unique_bone_name.utf8().get_data())) > 0; suffix++) {
			unique_bone_name = vformat("%s_%d", bone_name, suffix);
		}
		used_bone_names.insert(std::string(unique_bone_name.utf8().get_data()));

		const int bone_index = skeleton->add_bone(unique_bone_name);
		bone_index_by_joint_path[joints[joint_index].GetString()] = bone_index;
		skeleton->set_bone_meta(bone_index, StringName("usd_joint_path"), joint_path);
		skeleton->set_bone_meta(bone_index, StringName("usd_joint_index"), joint_index);
	}

	for (int joint_index = 0; joint_index < (int)joints.size(); joint_index++) {
		const String joint_path = to_godot_string(joints[joint_index].GetString());
		const String parent_joint = joint_parent_path(joint_path);
		const auto bone_it = bone_index_by_joint_path.find(joints[joint_index].GetString());
		ERR_CONTINUE(bone_it == bone_index_by_joint_path.end());
		const int bone_index = bone_it->second;

		if (!parent_joint.is_empty()) {
			const auto parent_it = bone_index_by_joint_path.find(parent_joint.utf8().get_data());
			if (parent_it != bone_index_by_joint_path.end()) {
				skeleton->set_bone_parent(bone_index, parent_it->second);
			} else if (r_mapping_notes != nullptr) {
				(*r_mapping_notes)["usd:mapping_status"] = "Skeleton joints referenced a missing parent path; unmatched joints were kept as skeleton roots.";
			}
			skeleton->set_bone_meta(bone_index, StringName("usd_joint_parent_path"), parent_joint);
		}
	}

	const Transform3D skeleton_world_inverse = (p_stage_correction_transform * gf_matrix_to_transform(UsdGeomXformable(p_prim).ComputeLocalToWorldTransform(p_time))).affine_inverse();
	if (!has_rest_transforms && has_bind_transforms && r_mapping_notes != nullptr) {
		(*r_mapping_notes)["usd:mapping_status"] = "Skeleton restTransforms were missing; local rest pose was approximated from bindTransforms.";
	}

	for (int joint_index = 0; joint_index < (int)joints.size(); joint_index++) {
		if (has_bind_transforms) {
			const Transform3D bind_transform = skeleton_world_inverse * (p_stage_correction_transform * gf_matrix_to_transform(bind_transforms[joint_index]));
			skeleton->set_bone_meta(joint_index, StringName("usd_joint_bind_transform"), bind_transform);
		}

		Transform3D local_rest;
		if (has_rest_transforms) {
			local_rest = gf_matrix_to_transform(rest_transforms[joint_index]);
		} else if (has_bind_transforms) {
			const String joint_path = to_godot_string(joints[joint_index].GetString());
			const String parent_joint = joint_parent_path(joint_path);
			const Transform3D bind_transform = skeleton_world_inverse * (p_stage_correction_transform * gf_matrix_to_transform(bind_transforms[joint_index]));
			if (parent_joint.is_empty()) {
				local_rest = bind_transform;
			} else {
				const auto parent_it = bone_index_by_joint_path.find(parent_joint.utf8().get_data());
				if (parent_it != bone_index_by_joint_path.end()) {
					const Transform3D parent_bind_transform = skeleton_world_inverse * (p_stage_correction_transform * gf_matrix_to_transform(bind_transforms[parent_it->second]));
					local_rest = parent_bind_transform.affine_inverse() * bind_transform;
				} else {
					local_rest = bind_transform;
				}
			}
		}

		skeleton->set_bone_rest(joint_index, local_rest);
	}

	skeleton->reset_bone_poses();
	return skeleton;
}

void append_skin_bindings(Node3D *p_scene_root) {
	ERR_FAIL_NULL(p_scene_root);

	std::vector<Node *> stack;
	stack.push_back(p_scene_root);
	while (!stack.empty()) {
		Node *node = stack.back();
		stack.pop_back();

		for (int i = 0; i < node->get_child_count(); i++) {
			stack.push_back(node->get_child(i));
		}

		MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(node);
		if (mesh_instance == nullptr || mesh_instance->get_mesh().is_null()) {
			continue;
		}

		const Dictionary metadata = get_usd_metadata(mesh_instance);
		const String skeleton_prim_path = metadata.get("usd:skel_skeleton_path", String());
		if (skeleton_prim_path.is_empty()) {
			continue;
		}

		Skeleton3D *skeleton = Object::cast_to<Skeleton3D>(find_node_for_prim_path(p_scene_root, skeleton_prim_path));
		if (skeleton == nullptr) {
			set_usd_metadata(mesh_instance, "usd:skinning_status", vformat("Skinned mesh referenced missing skeleton prim: %s", skeleton_prim_path));
			continue;
		}

		Transform3D geom_bind_transform;
		if (metadata.has("usd:skel_geom_bind_transform")) {
			geom_bind_transform = metadata["usd:skel_geom_bind_transform"];
		}

		mesh_instance->set_skin(build_mesh_skin(skeleton, geom_bind_transform));
		mesh_instance->set_skeleton_path(mesh_instance->get_path_to(skeleton));
	}
}

void append_baked_skeleton_animations(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, Node3D *p_scene_root) {
	ERR_FAIL_NULL(p_scene_root);
	ERR_FAIL_COND(p_stage == nullptr);

	for (const UsdPrim &prim : p_stage->Traverse()) {
		if (!prim.IsA<UsdSkelSkeleton>()) {
			continue;
		}

		Skeleton3D *skeleton = Object::cast_to<Skeleton3D>(find_node_for_prim_path(p_scene_root, to_godot_string(prim.GetPath().GetString())));
		if (skeleton == nullptr) {
			continue;
		}

		UsdRelationship animation_source_rel = prim.GetRelationship(TfToken("skel:animationSource"));
		if (!animation_source_rel) {
			continue;
		}

		SdfPathVector targets;
		animation_source_rel.GetTargets(&targets);
		for (const SdfPath &target : targets) {
			UsdPrim animation_prim = p_stage->GetPrimAtPath(target);
			if (!animation_prim || !animation_prim.IsA<UsdSkelAnimation>()) {
				continue;
			}
			append_baked_skeleton_animation(p_scene_root, skeleton, p_stage, p_time, UsdSkelAnimation(animation_prim));
		}
	}
}

} // namespace godot_usd
