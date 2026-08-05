// ---------------------------------------------------------------------------------------------
// Light dispatch ported from godot-usd (https://github.com/migueldeicaza) — MIT License,
// (c) 2026 Miguel de Icaza. See THIRD_PARTY.md.
// ---------------------------------------------------------------------------------------------
#include "usd_lights.h"
#include "usd_light_proxy.h"
#include "usd_stage_utils.h"   // set_usd_metadata
#include "usd_usdz.h"          // load_texture_from_asset_attribute

#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/spot_light3d.hpp>
#include <godot_cpp/classes/light3d.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/sphereLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/cylinderLight.h>
#include <pxr/usd/usdLux/shapingAPI.h>

namespace godot_usd {

using namespace godot;
using namespace pxr;

namespace {

template <typename TLight>
void apply_light_attributes(const TLight &p_light, const UsdTimeCode &p_time, Light3D *p_target) {
	ERR_FAIL_NULL(p_target);
	GfVec3f color_value(1.0f);
	float intensity = 1.0f;
	float exposure = 0.0f;
	p_light.GetColorAttr().Get(&color_value, p_time);
	p_light.GetIntensityAttr().Get(&intensity, p_time);
	p_light.GetExposureAttr().Get(&exposure, p_time);
	p_target->set_color(Color(color_value[0], color_value[1], color_value[2], 1.0f));
	p_target->set_param(Light3D::PARAM_INTENSITY, intensity * Math::pow(2.0f, exposure));
}

bool apply_shaping_to_spot_light(const UsdPrim &p_prim, const UsdTimeCode &p_time, SpotLight3D *p_target) {
	ERR_FAIL_NULL_V(p_target, false);
	if (!p_prim.HasAPI<UsdLuxShapingAPI>()) {
		return false;
	}
	UsdLuxShapingAPI shaping_api(p_prim);
	if (!shaping_api) {
		return false;
	}
	float cone_angle = 90.0f;
	shaping_api.GetShapingConeAngleAttr().Get(&cone_angle, p_time);
	p_target->set_param(Light3D::PARAM_SPOT_ANGLE, CLAMP(cone_angle, 0.1f, 90.0f));
	float cone_softness = 0.0f;
	shaping_api.GetShapingConeSoftnessAttr().Get(&cone_softness, p_time);
	p_target->set_param(Light3D::PARAM_SPOT_ATTENUATION, MAX(0.01f, 1.0f - CLAMP(cone_softness, 0.0f, 1.0f)));
	return true;
}

void apply_mapping_notes(Object *p_object, const Dictionary &p_mapping_notes) {
	Array keys = p_mapping_notes.keys();
	for (int i = 0; i < keys.size(); i++) {
		const Variant key = keys[i];
		if (key.get_type() == Variant::STRING || key.get_type() == Variant::STRING_NAME) {
			set_usd_metadata(p_object, String(key), p_mapping_notes[key]);
		}
	}
}

// Instantiate a native AreaLight3D via ClassDB if this build has one; nullptr otherwise (caller
// falls back to UsdAreaLightProxy). Matches godot-usd's runtime-tolerant approach.
Node *build_area_light3d_dynamic(const Vector2 &p_area_size, const Ref<Texture2D> &p_area_texture, float p_range, const Color &p_color, float p_intensity, const Dictionary &p_mapping_notes) {
	ClassDBSingleton *class_db = ClassDBSingleton::get_singleton();
	if (class_db == nullptr || !class_db->class_exists(StringName("AreaLight3D")) || !class_db->can_instantiate(StringName("AreaLight3D"))) {
		return nullptr;
	}
	Object *area_object = class_db->instantiate(StringName("AreaLight3D"));
	if (area_object == nullptr) {
		return nullptr;
	}
	Node *node = Object::cast_to<Node>(area_object);
	Light3D *light = Object::cast_to<Light3D>(area_object);
	if (node == nullptr || light == nullptr) {
		memdelete(area_object);
		return nullptr;
	}
	light->set_color(p_color);
	light->set_param(Light3D::PARAM_INTENSITY, p_intensity);
	light->set_param(Light3D::PARAM_RANGE, p_range);
	area_object->call(StringName("set_area_size"), p_area_size);
	if (p_area_texture.is_valid()) {
		area_object->call(StringName("set_area_texture"), p_area_texture);
	}
	apply_mapping_notes(area_object, p_mapping_notes);
	return node;
}

UsdAreaLightProxy *build_area_light_proxy(const String &p_source_schema, const String &p_shape, const Vector2 &p_area_size, const Ref<Texture2D> &p_area_texture, float p_range, const Color &p_color, float p_intensity, const Dictionary &p_mapping_notes) {
	UsdAreaLightProxy *proxy = memnew(UsdAreaLightProxy);
	proxy->set_source_schema(p_source_schema);
	proxy->set_light_shape(p_shape);
	proxy->set_area_size(p_area_size);
	proxy->set_area_texture(p_area_texture);
	proxy->set_light_color(p_color);
	proxy->set_light_intensity(p_intensity);
	proxy->set_light_range(p_range);
	apply_mapping_notes(proxy, p_mapping_notes);
	return proxy;
}

} // namespace

Node *build_light_node(const UsdStageRefPtr &p_stage, const UsdPrim &p_prim, const UsdTimeCode &p_time, Dictionary *r_mapping_notes) {
	Dictionary local_notes;
	Dictionary &notes = r_mapping_notes ? *r_mapping_notes : local_notes;

	if (p_prim.IsA<UsdLuxDistantLight>()) {
		DirectionalLight3D *light = memnew(DirectionalLight3D);
		apply_light_attributes(UsdLuxDistantLight(p_prim), p_time, light);
		return light;
	}
	if (p_prim.IsA<UsdLuxSphereLight>()) {
		UsdLuxSphereLight usd_light(p_prim);
		float radius = 1.0f;
		usd_light.GetRadiusAttr().Get(&radius, p_time);
		SpotLight3D *spot_light = memnew(SpotLight3D);
		if (apply_shaping_to_spot_light(p_prim, p_time, spot_light)) {
			apply_light_attributes(usd_light, p_time, spot_light);
			spot_light->set_param(Light3D::PARAM_SIZE, radius);
			spot_light->set_param(Light3D::PARAM_RANGE, MAX(radius * 20.0f, 1.0f));
			set_usd_metadata(spot_light, "usd:light_mapping", "UsdLuxSphereLight with ShapingAPI was approximated as SpotLight3D.");
			return spot_light;
		}
		memdelete(spot_light);
		OmniLight3D *light = memnew(OmniLight3D);
		apply_light_attributes(usd_light, p_time, light);
		light->set_param(Light3D::PARAM_SIZE, radius);
		light->set_param(Light3D::PARAM_RANGE, MAX(radius * 10.0f, 1.0f));
		return light;
	}
	if (p_prim.IsA<UsdLuxRectLight>()) {
		UsdLuxRectLight rect_light(p_prim);
		GfVec3f color(1.0f, 1.0f, 1.0f);
		float intensity = 1.0f, exposure = 0.0f, width = 1.0f, height = 1.0f;
		rect_light.GetColorAttr().Get(&color, p_time);
		rect_light.GetIntensityAttr().Get(&intensity, p_time);
		rect_light.GetExposureAttr().Get(&exposure, p_time);
		rect_light.GetWidthAttr().Get(&width, p_time);
		rect_light.GetHeightAttr().Get(&height, p_time);
		Ref<Texture2D> area_texture = load_texture_from_asset_attribute(p_stage, p_time, rect_light.GetTextureFileAttr());
		const Color light_color(color[0], color[1], color[2], 1.0f);
		const float light_intensity = intensity * Math::pow(2.0f, exposure);
		if (area_texture.is_valid()) {
			notes["usd:rect_light_has_texture"] = true;
		}
		Node *node = build_area_light3d_dynamic(Vector2(width, height), area_texture, MAX(MAX(width, height) * 10.0f, 1.0f), light_color, light_intensity, notes);
		if (node == nullptr) {
			notes["usd:light_mapping"] = "UsdLuxRectLight was mapped to UsdAreaLightProxy because AreaLight3D is unavailable in this runtime.";
			node = build_area_light_proxy("UsdLuxRectLight", "rect", Vector2(width, height), area_texture, MAX(MAX(width, height) * 10.0f, 1.0f), light_color, light_intensity, notes);
		}
		return node;
	}
	if (p_prim.IsA<UsdLuxDiskLight>()) {
		UsdLuxDiskLight disk_light(p_prim);
		GfVec3f color(1.0f, 1.0f, 1.0f);
		float intensity = 1.0f, exposure = 0.0f, radius = 0.5f;
		disk_light.GetColorAttr().Get(&color, p_time);
		disk_light.GetIntensityAttr().Get(&intensity, p_time);
		disk_light.GetExposureAttr().Get(&exposure, p_time);
		disk_light.GetRadiusAttr().Get(&radius, p_time);
		const Color light_color(color[0], color[1], color[2], 1.0f);
		const float light_intensity = intensity * Math::pow(2.0f, exposure);
		notes["usd:light_mapping"] = "UsdLuxDiskLight was approximated as AreaLight3D.";
		Node *node = build_area_light3d_dynamic(Vector2(radius * 2.0f, radius * 2.0f), Ref<Texture2D>(), MAX(radius * 10.0f, 1.0f), light_color, light_intensity, notes);
		if (node == nullptr) {
			notes["usd:light_mapping"] = "UsdLuxDiskLight was mapped to UsdAreaLightProxy because AreaLight3D is unavailable in this runtime.";
			node = build_area_light_proxy("UsdLuxDiskLight", "disk", Vector2(radius * 2.0f, radius * 2.0f), Ref<Texture2D>(), MAX(radius * 10.0f, 1.0f), light_color, light_intensity, notes);
		}
		return node;
	}
	if (p_prim.IsA<UsdLuxCylinderLight>()) {
		UsdLuxCylinderLight cylinder_light(p_prim);
		float radius = 0.5f, length = 1.0f;
		cylinder_light.GetRadiusAttr().Get(&radius, p_time);
		cylinder_light.GetLengthAttr().Get(&length, p_time);
		SpotLight3D *spot_light = memnew(SpotLight3D);
		if (apply_shaping_to_spot_light(p_prim, p_time, spot_light)) {
			apply_light_attributes(cylinder_light, p_time, spot_light);
			spot_light->set_param(Light3D::PARAM_SIZE, radius);
			spot_light->set_param(Light3D::PARAM_RANGE, MAX(length * 10.0f, 1.0f));
			set_usd_metadata(spot_light, "usd:light_mapping", "UsdLuxCylinderLight with ShapingAPI was approximated as SpotLight3D.");
			return spot_light;
		}
		memdelete(spot_light);
		OmniLight3D *light = memnew(OmniLight3D);
		apply_light_attributes(cylinder_light, p_time, light);
		light->set_param(Light3D::PARAM_SIZE, radius);
		light->set_param(Light3D::PARAM_RANGE, MAX(MAX(length, radius) * 10.0f, 1.0f));
		set_usd_metadata(light, "usd:light_mapping", "UsdLuxCylinderLight was approximated as OmniLight3D.");
		return light;
	}
	return nullptr;
}

} // namespace godot_usd
