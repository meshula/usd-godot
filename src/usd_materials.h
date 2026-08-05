// ---------------------------------------------------------------------------------------------
// Transplanted from godot-usd (https://github.com/migueldeicaza) — MIT License, (c) 2026 Miguel de
// Icaza. Harvested into usd-godot per USD_GODOT_INTEGRATION_PLAN.md (Phase 2), lifted near-verbatim
// and kept in the godot_usd namespace; adapted only as needed for Godot 4.7 / usd-godot. See
// THIRD_PARTY.md for provenance.
// ---------------------------------------------------------------------------------------------

#pragma once

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/shader.h>

namespace godot_usd {

using namespace godot;
using namespace pxr;

struct UsdShaderConnection {
	UsdShadeShader shader;
	TfToken output_name;

	UsdShaderConnection() = default;
	UsdShaderConnection(const UsdShadeShader &p_shader, const TfToken &p_output_name);

	operator bool() const;
};

UsdShaderConnection get_connected_shader(const UsdStageRefPtr &p_stage, const UsdShadeInput &p_input);
bool get_shader_id(const UsdShadeShader &p_shader, const UsdTimeCode &p_time, TfToken *r_shader_id);
UsdShaderConnection get_connected_texture_shader(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdShadeInput &p_input);
Ref<Image> load_image_from_shader(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdShadeShader &p_shader, String *r_resolved_path = nullptr);
Ref<Texture2D> load_texture_from_shader(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdShadeShader &p_shader, String *r_resolved_path = nullptr);
BaseMaterial3D::TextureChannel get_texture_channel_for_output(const TfToken &p_output_name);
void record_preview_texture_source(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, Dictionary *r_texture_sources, const String &p_input_name, const UsdShaderConnection &p_connection);
Ref<Image> make_opacity_composited_albedo(const Ref<Texture2D> &p_albedo_texture, const Ref<Image> &p_opacity_image, BaseMaterial3D::TextureChannel p_opacity_channel);
float preview_surface_f0_from_ior(float p_ior);
float godot_specular_from_preview_f0(float p_f0);
Ref<Material> build_material_from_usd_material(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdShadeMaterial &p_material, Dictionary *r_mapping_notes = nullptr);
bool write_preview_material(const UsdStageRefPtr &p_stage, const Ref<Material> &p_material, const SdfPath &p_mesh_path, const String &p_save_path, const String &p_material_key, UsdShadeMaterial *r_material);
Ref<Material> make_display_color_material(const Color &p_color, bool p_use_vertex_colors);

} // namespace godot_usd
