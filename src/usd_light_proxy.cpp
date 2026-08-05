// ---------------------------------------------------------------------------------------------
// Transplanted from godot-usd (https://github.com/migueldeicaza) — MIT License, (c) 2026 Miguel de
// Icaza. Harvested into usd-godot per USD_GODOT_INTEGRATION_PLAN.md (Phase 3), lifted near-verbatim.
// See THIRD_PARTY.md for provenance.
// ---------------------------------------------------------------------------------------------

#include "usd_light_proxy.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void UsdAreaLightProxy::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_area_size", "area_size"), &UsdAreaLightProxy::set_area_size);
	ClassDB::bind_method(D_METHOD("get_area_size"), &UsdAreaLightProxy::get_area_size);
	ClassDB::bind_method(D_METHOD("set_area_texture", "area_texture"), &UsdAreaLightProxy::set_area_texture);
	ClassDB::bind_method(D_METHOD("get_area_texture"), &UsdAreaLightProxy::get_area_texture);
	ClassDB::bind_method(D_METHOD("set_light_color", "light_color"), &UsdAreaLightProxy::set_light_color);
	ClassDB::bind_method(D_METHOD("get_light_color"), &UsdAreaLightProxy::get_light_color);
	ClassDB::bind_method(D_METHOD("set_light_intensity", "light_intensity"), &UsdAreaLightProxy::set_light_intensity);
	ClassDB::bind_method(D_METHOD("get_light_intensity"), &UsdAreaLightProxy::get_light_intensity);
	ClassDB::bind_method(D_METHOD("set_light_range", "light_range"), &UsdAreaLightProxy::set_light_range);
	ClassDB::bind_method(D_METHOD("get_light_range"), &UsdAreaLightProxy::get_light_range);
	ClassDB::bind_method(D_METHOD("set_light_shape", "light_shape"), &UsdAreaLightProxy::set_light_shape);
	ClassDB::bind_method(D_METHOD("get_light_shape"), &UsdAreaLightProxy::get_light_shape);
	ClassDB::bind_method(D_METHOD("set_source_schema", "source_schema"), &UsdAreaLightProxy::set_source_schema);
	ClassDB::bind_method(D_METHOD("get_source_schema"), &UsdAreaLightProxy::get_source_schema);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "area_size"), "set_area_size", "get_area_size");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "area_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_area_texture", "get_area_texture");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "light_color"), "set_light_color", "get_light_color");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "light_intensity"), "set_light_intensity", "get_light_intensity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "light_range"), "set_light_range", "get_light_range");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "light_shape"), "set_light_shape", "get_light_shape");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_schema"), "set_source_schema", "get_source_schema");
}

void UsdAreaLightProxy::set_area_size(const Vector2 &p_area_size) {
	area_size = p_area_size;
}

Vector2 UsdAreaLightProxy::get_area_size() const {
	return area_size;
}

void UsdAreaLightProxy::set_area_texture(const Ref<Texture2D> &p_area_texture) {
	area_texture = p_area_texture;
}

Ref<Texture2D> UsdAreaLightProxy::get_area_texture() const {
	return area_texture;
}

void UsdAreaLightProxy::set_light_color(const Color &p_light_color) {
	light_color = p_light_color;
}

Color UsdAreaLightProxy::get_light_color() const {
	return light_color;
}

void UsdAreaLightProxy::set_light_intensity(float p_light_intensity) {
	light_intensity = p_light_intensity;
}

float UsdAreaLightProxy::get_light_intensity() const {
	return light_intensity;
}

void UsdAreaLightProxy::set_light_range(float p_light_range) {
	light_range = p_light_range;
}

float UsdAreaLightProxy::get_light_range() const {
	return light_range;
}

void UsdAreaLightProxy::set_light_shape(const String &p_light_shape) {
	light_shape = p_light_shape;
}

String UsdAreaLightProxy::get_light_shape() const {
	return light_shape;
}

void UsdAreaLightProxy::set_source_schema(const String &p_source_schema) {
	source_schema = p_source_schema;
}

String UsdAreaLightProxy::get_source_schema() const {
	return source_schema;
}
