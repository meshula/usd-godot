// ---------------------------------------------------------------------------------------------
// Transplanted from godot-usd (https://github.com/migueldeicaza) — MIT License, (c) 2026 Miguel de
// Icaza. Harvested into usd-godot per USD_GODOT_INTEGRATION_PLAN.md (Phase 3), lifted near-verbatim.
// See THIRD_PARTY.md for provenance.
// ---------------------------------------------------------------------------------------------

#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>

using namespace godot;

class UsdAreaLightProxy : public Node3D {
	GDCLASS(UsdAreaLightProxy, Node3D);

	Vector2 area_size = Vector2(1.0, 1.0);
	Ref<Texture2D> area_texture;
	Color light_color = Color(1.0, 1.0, 1.0, 1.0);
	float light_intensity = 1.0f;
	float light_range = 1.0f;
	String light_shape = "rect";
	String source_schema;

protected:
	static void _bind_methods();

public:
	void set_area_size(const Vector2 &p_area_size);
	Vector2 get_area_size() const;

	void set_area_texture(const Ref<Texture2D> &p_area_texture);
	Ref<Texture2D> get_area_texture() const;

	void set_light_color(const Color &p_light_color);
	Color get_light_color() const;

	void set_light_intensity(float p_light_intensity);
	float get_light_intensity() const;

	void set_light_range(float p_light_range);
	float get_light_range() const;

	void set_light_shape(const String &p_light_shape);
	String get_light_shape() const;

	void set_source_schema(const String &p_source_schema);
	String get_source_schema() const;
};
