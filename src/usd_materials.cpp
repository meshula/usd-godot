// ---------------------------------------------------------------------------------------------
// Transplanted from godot-usd (https://github.com/migueldeicaza) — MIT License, (c) 2026 Miguel de
// Icaza. Harvested into usd-godot per USD_GODOT_INTEGRATION_PLAN.md (Phase 2), lifted near-verbatim
// and kept in the godot_usd namespace; adapted only as needed for Godot 4.7 / usd-godot. See
// THIRD_PARTY.md for provenance.
// ---------------------------------------------------------------------------------------------

#include "usd_materials.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <pxr/base/tf/stringUtils.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/shader.h>

#include "usd_stage_utils.h"
#include "usd_usdz.h"

namespace godot_usd {

using namespace godot;
using namespace pxr;

namespace {

bool extract_texture_uv_transform(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdShadeShader &p_texture_shader, Vector3 *r_uv_scale, Vector3 *r_uv_offset, Dictionary *r_mapping_notes) {
	if (p_stage == nullptr || !p_texture_shader || r_uv_scale == nullptr || r_uv_offset == nullptr) {
		return false;
	}

	UsdShadeInput st_input = p_texture_shader.GetInput(TfToken("st"));
	const UsdShaderConnection st_connection = get_connected_shader(p_stage, st_input);
	if (!st_connection) {
		return false;
	}

	TfToken shader_id;
	if (!get_shader_id(st_connection.shader, p_time, &shader_id) || shader_id != TfToken("UsdTransform2d")) {
		return false;
	}

	GfVec2f scale(1.0f, 1.0f);
	if (UsdShadeInput scale_input = st_connection.shader.GetInput(TfToken("scale"))) {
		scale_input.Get(&scale, p_time);
	}

	GfVec2f translation(0.0f, 0.0f);
	if (UsdShadeInput translation_input = st_connection.shader.GetInput(TfToken("translation"))) {
		translation_input.Get(&translation, p_time);
	}

	float rotation = 0.0f;
	if (UsdShadeInput rotation_input = st_connection.shader.GetInput(TfToken("rotation"))) {
		rotation_input.Get(&rotation, p_time);
	}

	if (!Math::is_zero_approx(rotation) && r_mapping_notes != nullptr) {
		(*r_mapping_notes)["usd:material_uv_transform"] = "UsdTransform2d rotation is not supported by StandardMaterial3D; only scale and translation were applied.";
	}

	*r_uv_scale = Vector3((real_t)scale[0], (real_t)scale[1], 1.0f);
	*r_uv_offset = Vector3((real_t)translation[0], (real_t)(1.0f - scale[1] - translation[1]), 0.0f);
	return true;
}

void merge_material_uv_transform(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdShadeShader &p_texture_shader, StandardMaterial3D *p_material, bool *r_has_uv_transform, Vector3 *r_uv_scale, Vector3 *r_uv_offset, Dictionary *r_mapping_notes) {
	ERR_FAIL_NULL(p_material);
	ERR_FAIL_NULL(r_has_uv_transform);
	ERR_FAIL_NULL(r_uv_scale);
	ERR_FAIL_NULL(r_uv_offset);

	Vector3 uv_scale;
	Vector3 uv_offset;
	if (!extract_texture_uv_transform(p_stage, p_time, p_texture_shader, &uv_scale, &uv_offset, r_mapping_notes)) {
		return;
	}

	if (!*r_has_uv_transform) {
		*r_has_uv_transform = true;
		*r_uv_scale = uv_scale;
		*r_uv_offset = uv_offset;
		p_material->set_uv1_scale(uv_scale);
		p_material->set_uv1_offset(uv_offset);
		return;
	}

	if (!r_uv_scale->is_equal_approx(uv_scale) || !r_uv_offset->is_equal_approx(uv_offset)) {
		if (r_mapping_notes != nullptr) {
			(*r_mapping_notes)["usd:material_uv_transform"] = "Multiple incompatible UsdTransform2d nodes were found; the first transform was kept.";
		}
	}
}

bool is_texture_output_channel(const TfToken &p_output_name, BaseMaterial3D::TextureChannel p_channel) {
	return get_texture_channel_for_output(p_output_name) == p_channel;
}

String make_valid_identifier(const String &p_name) {
	std::string valid = TfMakeValidIdentifier(p_name.is_empty() ? "Material" : p_name.utf8().get_data());
	if (valid.empty()) {
		valid = "Material";
	}
	return to_godot_string(valid);
}

float preview_f0_from_godot_specular(float p_specular) {
	const float clamped_specular = CLAMP(p_specular, 0.0f, 1.0f);
	return 0.16f * clamped_specular * clamped_specular;
}

Dictionary get_preview_surface_texture_sources(const Object *p_object) {
	if (p_object == nullptr) {
		return Dictionary();
	}
	return get_usd_metadata(p_object).get("usd:preview_surface_texture_sources", Dictionary());
}

Dictionary get_preview_surface_texture_source(const Object *p_object, const String &p_input_name) {
	const Dictionary texture_sources = get_preview_surface_texture_sources(p_object);
	if (!texture_sources.has(p_input_name)) {
		return Dictionary();
	}
	const Variant source = texture_sources[p_input_name];
	return source.get_type() == Variant::DICTIONARY ? (Dictionary)source : Dictionary();
}

TfToken get_usd_texture_output_for_name(const String &p_output_name) {
	if (p_output_name == "r" || p_output_name == "red") {
		return TfToken("r");
	}
	if (p_output_name == "g" || p_output_name == "green") {
		return TfToken("g");
	}
	if (p_output_name == "b" || p_output_name == "blue") {
		return TfToken("b");
	}
	if (p_output_name == "a" || p_output_name == "alpha") {
		return TfToken("a");
	}
	return TfToken("rgb");
}

SdfValueTypeName get_usd_output_type_for_name(const String &p_output_name) {
	return get_usd_texture_output_for_name(p_output_name) == TfToken("rgb") ? SdfValueTypeNames->Float3 : SdfValueTypeNames->Float;
}

TfToken get_usd_texture_output_for_channel(BaseMaterial3D::TextureChannel p_channel) {
	switch (p_channel) {
		case BaseMaterial3D::TEXTURE_CHANNEL_RED:
			return TfToken("r");
		case BaseMaterial3D::TEXTURE_CHANNEL_GREEN:
			return TfToken("g");
		case BaseMaterial3D::TEXTURE_CHANNEL_BLUE:
			return TfToken("b");
		case BaseMaterial3D::TEXTURE_CHANNEL_ALPHA:
			return TfToken("a");
		case BaseMaterial3D::TEXTURE_CHANNEL_GRAYSCALE:
		default:
			return TfToken("rgb");
	}
}

SdfValueTypeName get_usd_output_type_for_channel(BaseMaterial3D::TextureChannel p_channel) {
	return p_channel == BaseMaterial3D::TEXTURE_CHANNEL_GRAYSCALE ? SdfValueTypeNames->Float3 : SdfValueTypeNames->Float;
}

bool write_texture_uv_transform(const BaseMaterial3D *p_material, const UsdStageRefPtr &p_stage, UsdShadeShader p_texture_shader, const SdfPath &p_shader_path) {
	ERR_FAIL_NULL_V(p_material, false);
	if (!p_texture_shader || p_stage == nullptr) {
		return false;
	}

	const Vector3 uv_scale = p_material->get_uv1_scale();
	const Vector3 uv_offset = p_material->get_uv1_offset();
	if (uv_scale.is_equal_approx(Vector3(1.0f, 1.0f, 1.0f)) && uv_offset.is_equal_approx(Vector3())) {
		return false;
	}

	UsdShadeShader uv_transform = UsdShadeShader::Define(p_stage, p_shader_path.AppendChild(TfToken("UVTransform")));
	uv_transform.CreateIdAttr(VtValue(TfToken("UsdTransform2d")));
	uv_transform.CreateInput(TfToken("scale"), SdfValueTypeNames->Float2).Set(GfVec2f(uv_scale.x, uv_scale.y));
	uv_transform.CreateInput(TfToken("translation"), SdfValueTypeNames->Float2).Set(GfVec2f(uv_offset.x, 1.0f - uv_scale.y - uv_offset.y));
	uv_transform.CreateInput(TfToken("rotation"), SdfValueTypeNames->Float).Set(0.0f);

	UsdShadeOutput uv_output = uv_transform.CreateOutput(TfToken("result"), SdfValueTypeNames->Float2);
	p_texture_shader.CreateInput(TfToken("st"), SdfValueTypeNames->Float2).ConnectToSource(uv_output);
	return true;
}

String get_generated_texture_asset_path(const Ref<Texture2D> &p_texture, const String &p_save_path, const String &p_generated_asset_name) {
	if (p_texture.is_null()) {
		return String();
	}

	Ref<Image> image = p_texture->get_image();
	if (image.is_null()) {
		return String();
	}

	image = image->duplicate(true);
	if (image->is_compressed()) {
		image->decompress();
	}
	if (image->get_format() != Image::FORMAT_RGBA8) {
		image->convert(Image::FORMAT_RGBA8);
	}

	const String absolute_save_path = get_absolute_path(p_save_path);
	const String save_dir = absolute_save_path.get_base_dir();
	const String save_stem = absolute_save_path.get_file().get_basename();
	const String asset_dir_name = make_valid_identifier(save_stem).to_lower() + "_assets";
	const String asset_file_name = make_valid_identifier(p_generated_asset_name).to_lower() + ".png";
	const String absolute_asset_dir = save_dir.path_join(asset_dir_name);
	const Error mkdir_error = DirAccess::make_dir_recursive_absolute(absolute_asset_dir);
	if (mkdir_error != OK) {
		return String();
	}

	const String absolute_asset_path = absolute_asset_dir.path_join(asset_file_name);
	if (image->save_png(absolute_asset_path) != OK) {
		return String();
	}

	return asset_dir_name.path_join(asset_file_name);
}

bool connect_preview_texture_asset_path(const UsdStageRefPtr &p_stage, const String &p_asset_path, const BaseMaterial3D *p_material, const SdfPath &p_material_path, const char *p_shader_name, const char *p_input_name, const SdfValueTypeName &p_input_type, const TfToken &p_output_name, const SdfValueTypeName &p_output_type) {
	ERR_FAIL_NULL_V(p_material, false);
	if (p_stage == nullptr || p_asset_path.is_empty()) {
		return false;
	}

	UsdShadeShader preview_surface = UsdShadeShader::Get(p_stage, p_material_path.AppendChild(TfToken("PreviewSurface")));
	if (!preview_surface) {
		return false;
	}

	const SdfPath texture_shader_path = p_material_path.AppendChild(TfToken(p_shader_name));
	UsdShadeShader texture_shader = UsdShadeShader::Define(p_stage, texture_shader_path);
	texture_shader.CreateIdAttr(VtValue(TfToken("UsdUVTexture")));
	texture_shader.CreateInput(TfToken("file"), SdfValueTypeNames->Asset).Set(SdfAssetPath(p_asset_path.utf8().get_data()));
	write_texture_uv_transform(p_material, p_stage, texture_shader, texture_shader_path);

	UsdShadeOutput texture_output = texture_shader.CreateOutput(p_output_name, p_output_type);
	preview_surface.CreateInput(TfToken(p_input_name), p_input_type).ConnectToSource(texture_output);
	return true;
}

bool connect_preview_texture(const UsdStageRefPtr &p_stage, const String &p_save_path, const BaseMaterial3D *p_material, const Ref<Texture2D> &p_texture, const SdfPath &p_material_path, const char *p_shader_name, const char *p_input_name, const SdfValueTypeName &p_input_type, const TfToken &p_output_name, const SdfValueTypeName &p_output_type) {
	ERR_FAIL_NULL_V(p_material, false);
	if (p_texture.is_null()) {
		return false;
	}

	const String generated_asset_name = vformat("%s_%s", make_valid_identifier(to_godot_string(p_material_path.GetName())), make_valid_identifier(String(p_shader_name)));
	const String asset_path = get_generated_texture_asset_path(p_texture, p_save_path, generated_asset_name);
	if (asset_path.is_empty()) {
		return false;
	}

	return connect_preview_texture_asset_path(p_stage, asset_path, p_material, p_material_path, p_shader_name, p_input_name, p_input_type, p_output_name, p_output_type);
}

} // namespace

UsdShaderConnection::UsdShaderConnection(const UsdShadeShader &p_shader, const TfToken &p_output_name) :
		shader(p_shader),
		output_name(p_output_name) {}

UsdShaderConnection::operator bool() const {
	return static_cast<bool>(shader);
}

UsdShaderConnection get_connected_shader(const UsdStageRefPtr &p_stage, const UsdShadeInput &p_input) {
	if (p_stage == nullptr || !p_input || !p_input.HasConnectedSource()) {
		return UsdShaderConnection();
	}

	const UsdShadeInput::SourceInfoVector sources = p_input.GetConnectedSources();
	if (sources.empty()) {
		return UsdShaderConnection();
	}

	UsdPrim source_prim = p_stage->GetPrimAtPath(sources[0].source.GetPath());
	if (!source_prim) {
		return UsdShaderConnection();
	}

	UsdShadeShader shader(source_prim);
	if (!shader) {
		return UsdShaderConnection();
	}

	return UsdShaderConnection(shader, sources[0].sourceName);
}

bool get_shader_id(const UsdShadeShader &p_shader, const UsdTimeCode &p_time, TfToken *r_shader_id) {
	if (!p_shader || r_shader_id == nullptr) {
		return false;
	}

	return p_shader.GetPrim().GetAttribute(TfToken("info:id")).Get(r_shader_id, p_time);
}

UsdShaderConnection get_connected_texture_shader(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdShadeInput &p_input) {
	const UsdShaderConnection connection = get_connected_shader(p_stage, p_input);
	if (!connection) {
		return UsdShaderConnection();
	}

	TfToken shader_id;
	if (!get_shader_id(connection.shader, p_time, &shader_id) || shader_id != TfToken("UsdUVTexture")) {
		return UsdShaderConnection();
	}

	return connection;
}

Ref<Image> load_image_from_shader(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdShadeShader &p_shader, String *r_resolved_path) {
	UsdShadeInput file_input = p_shader.GetInput(TfToken("file"));
	if (!file_input) {
		return Ref<Image>();
	}
	return load_image_from_asset_attribute(p_stage, p_time, file_input.GetAttr(), r_resolved_path);
}

Ref<Texture2D> load_texture_from_shader(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdShadeShader &p_shader, String *r_resolved_path) {
	UsdShadeInput file_input = p_shader.GetInput(TfToken("file"));
	if (!file_input) {
		return Ref<Texture2D>();
	}
	return load_texture_from_asset_attribute(p_stage, p_time, file_input.GetAttr(), r_resolved_path);
}

BaseMaterial3D::TextureChannel get_texture_channel_for_output(const TfToken &p_output_name) {
	if (p_output_name == TfToken("r") || p_output_name == TfToken("red")) {
		return BaseMaterial3D::TEXTURE_CHANNEL_RED;
	}
	if (p_output_name == TfToken("g") || p_output_name == TfToken("green")) {
		return BaseMaterial3D::TEXTURE_CHANNEL_GREEN;
	}
	if (p_output_name == TfToken("b") || p_output_name == TfToken("blue")) {
		return BaseMaterial3D::TEXTURE_CHANNEL_BLUE;
	}
	if (p_output_name == TfToken("a") || p_output_name == TfToken("alpha")) {
		return BaseMaterial3D::TEXTURE_CHANNEL_ALPHA;
	}
	return BaseMaterial3D::TEXTURE_CHANNEL_GRAYSCALE;
}

void record_preview_texture_source(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, Dictionary *r_texture_sources, const String &p_input_name, const UsdShaderConnection &p_connection) {
	ERR_FAIL_NULL(r_texture_sources);
	if (p_stage == nullptr || !p_connection) {
		return;
	}

	UsdShadeInput file_input = p_connection.shader.GetInput(TfToken("file"));
	if (!file_input) {
		return;
	}

	SdfAssetPath asset_path;
	if (!file_input.Get(&asset_path, p_time)) {
		return;
	}

	const String authored_path = normalize_usd_asset_path(to_godot_string(asset_path.GetAssetPath()));
	const String resolved_path = resolve_asset_path(p_stage, asset_path);
	if (authored_path.is_empty() && resolved_path.is_empty()) {
		return;
	}

	Dictionary source_description;
	source_description["asset_path"] = authored_path.is_empty() ? resolved_path : authored_path;
	source_description["output_name"] = to_godot_string(p_connection.output_name.GetString());
	source_description["shader_path"] = to_godot_string(p_connection.shader.GetPath().GetString());
	(*r_texture_sources)[p_input_name] = source_description;
}

Ref<Image> make_opacity_composited_albedo(const Ref<Texture2D> &p_albedo_texture, const Ref<Image> &p_opacity_image, BaseMaterial3D::TextureChannel p_opacity_channel) {
	if (p_opacity_image.is_null()) {
		return Ref<Image>();
	}

	Ref<Image> opacity_image = p_opacity_image->duplicate(true);
	if (opacity_image.is_null()) {
		return Ref<Image>();
	}
	if (opacity_image->is_compressed()) {
		opacity_image->decompress();
	}
	opacity_image->convert(Image::FORMAT_RGBA8);

	Ref<Image> albedo_image;
	if (p_albedo_texture.is_valid()) {
		albedo_image = p_albedo_texture->get_image();
	}
	if (albedo_image.is_valid()) {
		albedo_image = albedo_image->duplicate(true);
		if (albedo_image->is_compressed()) {
			albedo_image->decompress();
		}
		albedo_image->convert(Image::FORMAT_RGBA8);
	} else {
		albedo_image = Image::create_empty(opacity_image->get_width(), opacity_image->get_height(), false, Image::FORMAT_RGBA8);
		albedo_image->fill(Color(1.0f, 1.0f, 1.0f, 1.0f));
	}

	if (opacity_image->get_width() != albedo_image->get_width() || opacity_image->get_height() != albedo_image->get_height()) {
		opacity_image->resize(albedo_image->get_width(), albedo_image->get_height(), Image::INTERPOLATE_BILINEAR);
	}

	for (int y = 0; y < albedo_image->get_height(); y++) {
		for (int x = 0; x < albedo_image->get_width(); x++) {
			Color albedo = albedo_image->get_pixel(x, y);
			const Color opacity = opacity_image->get_pixel(x, y);
			switch (p_opacity_channel) {
				case BaseMaterial3D::TEXTURE_CHANNEL_RED:
					albedo.a = opacity.r;
					break;
				case BaseMaterial3D::TEXTURE_CHANNEL_GREEN:
					albedo.a = opacity.g;
					break;
				case BaseMaterial3D::TEXTURE_CHANNEL_BLUE:
					albedo.a = opacity.b;
					break;
				case BaseMaterial3D::TEXTURE_CHANNEL_ALPHA:
					albedo.a = opacity.a;
					break;
				case BaseMaterial3D::TEXTURE_CHANNEL_GRAYSCALE:
				default:
					albedo.a = opacity.get_luminance();
					break;
			}
			albedo_image->set_pixel(x, y, albedo);
		}
	}

	return albedo_image;
}

float preview_surface_f0_from_ior(float p_ior) {
	if (p_ior <= 0.0f) {
		return 0.04f;
	}

	const float reflectance = (p_ior - 1.0f) / (p_ior + 1.0f);
	return reflectance * reflectance;
}

float godot_specular_from_preview_f0(float p_f0) {
	return CLAMP(Math::sqrt(MAX(p_f0, 0.0f) / 0.16f), 0.0f, 1.0f);
}

Ref<Material> build_material_from_usd_material(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdShadeMaterial &p_material, Dictionary *r_mapping_notes) {
	if (p_stage == nullptr || !p_material) {
		return Ref<Material>();
	}

	const String material_path = to_godot_string(p_material.GetPath().GetString());
	UsdShadeShader preview_surface = p_material.ComputeSurfaceSource();
	if (preview_surface) {
		preview_surface = UsdShadeShader(p_stage->GetPrimAtPath(preview_surface.GetPath()));
	}
	if (!preview_surface) {
		if (r_mapping_notes != nullptr) {
			(*r_mapping_notes)["usd:material_status"] = vformat("Material %s has no supported surface source.", material_path);
		}
		return Ref<Material>();
	}

	TfToken shader_id;
	preview_surface.GetPrim().GetAttribute(TfToken("info:id")).Get(&shader_id, p_time);
	if (shader_id != TfToken("UsdPreviewSurface")) {
		if (r_mapping_notes != nullptr) {
			(*r_mapping_notes)["usd:material_status"] = vformat("Material %s uses unsupported shader id %s.", material_path, to_godot_string(shader_id.GetString()));
		}
		return Ref<Material>();
	}

	Ref<StandardMaterial3D> material;
	material.instantiate();
	set_usd_metadata(material.ptr(), "usd:material_path", material_path);
	bool has_uv_transform = false;
	Vector3 uv_scale;
	Vector3 uv_offset;
	Dictionary texture_sources;

	UsdShadeInput diffuse_input = preview_surface.GetInput(TfToken("diffuseColor"));
	if (diffuse_input) {
		GfVec3f diffuse_color(1.0f, 1.0f, 1.0f);
		diffuse_input.Get(&diffuse_color, p_time);
		material->set_albedo(Color(diffuse_color[0], diffuse_color[1], diffuse_color[2], 1.0f));

		const UsdShaderConnection texture_connection = get_connected_texture_shader(p_stage, p_time, diffuse_input);
		if (texture_connection) {
			record_preview_texture_source(p_stage, p_time, &texture_sources, "diffuseColor", texture_connection);
			Ref<Texture2D> texture = load_texture_from_shader(p_stage, p_time, texture_connection.shader);
			if (texture.is_valid()) {
				material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, texture);
				merge_material_uv_transform(p_stage, p_time, texture_connection.shader, material.ptr(), &has_uv_transform, &uv_scale, &uv_offset, r_mapping_notes);
			}
		} else if (diffuse_input.HasConnectedSource() && r_mapping_notes != nullptr) {
			(*r_mapping_notes)["usd:material_status"] = vformat("Material %s uses an unsupported diffuseColor source shader.", material_path);
		}
	}

	float metallic = 0.0f;
	UsdShadeInput metallic_input = preview_surface.GetInput(TfToken("metallic"));
	if (metallic_input && metallic_input.Get(&metallic, p_time)) {
		material->set_metallic(metallic);
	}
	if (metallic_input && metallic_input.HasConnectedSource()) {
		const UsdShaderConnection texture_connection = get_connected_texture_shader(p_stage, p_time, metallic_input);
		if (texture_connection) {
			record_preview_texture_source(p_stage, p_time, &texture_sources, "metallic", texture_connection);
			Ref<Texture2D> texture = load_texture_from_shader(p_stage, p_time, texture_connection.shader);
			if (texture.is_valid()) {
				material->set_texture(BaseMaterial3D::TEXTURE_METALLIC, texture);
				material->set_metallic_texture_channel(get_texture_channel_for_output(texture_connection.output_name));
				merge_material_uv_transform(p_stage, p_time, texture_connection.shader, material.ptr(), &has_uv_transform, &uv_scale, &uv_offset, r_mapping_notes);
			}
		} else if (r_mapping_notes != nullptr) {
			(*r_mapping_notes)["usd:material_status"] = vformat("Material %s uses an unsupported metallic source shader.", material_path);
		}
	}

	float roughness = 0.5f;
	UsdShadeInput roughness_input = preview_surface.GetInput(TfToken("roughness"));
	if (roughness_input && roughness_input.Get(&roughness, p_time)) {
		material->set_roughness(roughness);
	}
	if (roughness_input && roughness_input.HasConnectedSource()) {
		const UsdShaderConnection texture_connection = get_connected_texture_shader(p_stage, p_time, roughness_input);
		if (texture_connection) {
			record_preview_texture_source(p_stage, p_time, &texture_sources, "roughness", texture_connection);
			Ref<Texture2D> texture = load_texture_from_shader(p_stage, p_time, texture_connection.shader);
			if (texture.is_valid()) {
				material->set_texture(BaseMaterial3D::TEXTURE_ROUGHNESS, texture);
				material->set_roughness_texture_channel(get_texture_channel_for_output(texture_connection.output_name));
				merge_material_uv_transform(p_stage, p_time, texture_connection.shader, material.ptr(), &has_uv_transform, &uv_scale, &uv_offset, r_mapping_notes);
			}
		} else if (r_mapping_notes != nullptr) {
			(*r_mapping_notes)["usd:material_status"] = vformat("Material %s uses an unsupported roughness source shader.", material_path);
		}
	}

	UsdShadeInput normal_input = preview_surface.GetInput(TfToken("normal"));
	if (normal_input && normal_input.HasConnectedSource()) {
		const UsdShaderConnection texture_connection = get_connected_texture_shader(p_stage, p_time, normal_input);
		if (texture_connection) {
			record_preview_texture_source(p_stage, p_time, &texture_sources, "normal", texture_connection);
			Ref<Texture2D> texture = load_texture_from_shader(p_stage, p_time, texture_connection.shader);
			if (texture.is_valid()) {
				material->set_feature(BaseMaterial3D::FEATURE_NORMAL_MAPPING, true);
				material->set_texture(BaseMaterial3D::TEXTURE_NORMAL, texture);
				merge_material_uv_transform(p_stage, p_time, texture_connection.shader, material.ptr(), &has_uv_transform, &uv_scale, &uv_offset, r_mapping_notes);
			}
		} else if (r_mapping_notes != nullptr) {
			(*r_mapping_notes)["usd:material_status"] = vformat("Material %s uses an unsupported normal source shader.", material_path);
		}
	}

	bool has_opacity = false;
	float opacity = 1.0f;
	UsdShadeInput opacity_input = preview_surface.GetInput(TfToken("opacity"));
	if (opacity_input && opacity_input.Get(&opacity, p_time)) {
		if (opacity < 1.0f) {
			Color albedo = material->get_albedo();
			albedo.a = opacity;
			material->set_albedo(albedo);
			material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
			has_opacity = true;
		}
	}
	if (opacity_input && opacity_input.HasConnectedSource()) {
		const UsdShaderConnection texture_connection = get_connected_texture_shader(p_stage, p_time, opacity_input);
		if (texture_connection) {
			record_preview_texture_source(p_stage, p_time, &texture_sources, "opacity", texture_connection);
			Ref<Image> opacity_image = load_image_from_shader(p_stage, p_time, texture_connection.shader);
			if (opacity_image.is_valid()) {
				const Ref<Image> composited_albedo = make_opacity_composited_albedo(material->get_texture(BaseMaterial3D::TEXTURE_ALBEDO), opacity_image, get_texture_channel_for_output(texture_connection.output_name));
				if (composited_albedo.is_valid()) {
					material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, texture_from_image(composited_albedo));
					material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
					has_opacity = true;
				}
				merge_material_uv_transform(p_stage, p_time, texture_connection.shader, material.ptr(), &has_uv_transform, &uv_scale, &uv_offset, r_mapping_notes);
			}
		} else if (r_mapping_notes != nullptr) {
			(*r_mapping_notes)["usd:material_status"] = vformat("Material %s uses an unsupported opacity source shader.", material_path);
		}
	}

	float opacity_threshold = 0.0f;
	UsdShadeInput opacity_threshold_input = preview_surface.GetInput(TfToken("opacityThreshold"));
	if (opacity_threshold_input && opacity_threshold_input.Get(&opacity_threshold, p_time) && opacity_threshold > 0.0f) {
		material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
		material->set_alpha_scissor_threshold(CLAMP(opacity_threshold, 0.0f, 1.0f));
	} else if (has_opacity) {
		material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	}

	GfVec3f emission(0.0f, 0.0f, 0.0f);
	UsdShadeInput emission_input = preview_surface.GetInput(TfToken("emissiveColor"));
	bool has_emission = false;
	if (emission_input && emission_input.Get(&emission, p_time)) {
		const Color emission_color(emission[0], emission[1], emission[2], 1.0f);
		if (emission_color.r > 0.0f || emission_color.g > 0.0f || emission_color.b > 0.0f) {
			material->set_emission(emission_color);
			has_emission = true;
		}
	}
	if (emission_input && emission_input.HasConnectedSource()) {
		const UsdShaderConnection texture_connection = get_connected_texture_shader(p_stage, p_time, emission_input);
		if (texture_connection) {
			record_preview_texture_source(p_stage, p_time, &texture_sources, "emissiveColor", texture_connection);
			Ref<Texture2D> texture = load_texture_from_shader(p_stage, p_time, texture_connection.shader);
			if (texture.is_valid()) {
				material->set_texture(BaseMaterial3D::TEXTURE_EMISSION, texture);
				has_emission = true;
				merge_material_uv_transform(p_stage, p_time, texture_connection.shader, material.ptr(), &has_uv_transform, &uv_scale, &uv_offset, r_mapping_notes);
			}
		} else if (r_mapping_notes != nullptr) {
			(*r_mapping_notes)["usd:material_status"] = vformat("Material %s uses an unsupported emissiveColor source shader.", material_path);
		}
	}
	if (has_emission) {
		material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
		material->set_emission_energy_multiplier(1.0f);
	}

	bool use_specular_workflow = false;
	if (UsdShadeInput use_specular_workflow_input = preview_surface.GetInput(TfToken("useSpecularWorkflow"))) {
		use_specular_workflow_input.Get(&use_specular_workflow, p_time);
	}
	if (use_specular_workflow) {
		set_usd_metadata(material.ptr(), "usd:preview_surface_use_specular_workflow", true);
	}

	bool has_ior = false;
	float ior = 1.5f;
	if (UsdShadeInput ior_input = preview_surface.GetInput(TfToken("ior"))) {
		has_ior = ior_input.Get(&ior, p_time);
		if (has_ior) {
			set_usd_metadata(material.ptr(), "usd:preview_surface_ior", ior);
		}
	}

	UsdShadeInput specular_input = preview_surface.GetInput(TfToken("specularColor"));
	bool applied_specular_override = false;
	if (use_specular_workflow && specular_input) {
		if (specular_input.HasConnectedSource()) {
			const UsdShaderConnection texture_connection = get_connected_texture_shader(p_stage, p_time, specular_input);
			if (texture_connection) {
				record_preview_texture_source(p_stage, p_time, &texture_sources, "specularColor", texture_connection);
				if (r_mapping_notes != nullptr) {
					(*r_mapping_notes)["usd:material_status"] = vformat("Material %s uses specularColor texture input; it is preserved for save, but only approximated for Godot display.", material_path);
				}
			} else if (r_mapping_notes != nullptr) {
				(*r_mapping_notes)["usd:material_status"] = vformat("Material %s uses an unsupported specularColor source shader.", material_path);
			}
		}

		GfVec3f specular_color(0.04f, 0.04f, 0.04f);
		if (specular_input.Get(&specular_color, p_time)) {
			const Color specular_preview_color(specular_color[0], specular_color[1], specular_color[2], 1.0f);
			set_usd_metadata(material.ptr(), "usd:preview_surface_specular_color", specular_preview_color);
			material->set_specular(godot_specular_from_preview_f0(specular_preview_color.get_luminance()));
			applied_specular_override = true;
		}
	}
	if (!applied_specular_override && has_ior) {
		material->set_specular(godot_specular_from_preview_f0(preview_surface_f0_from_ior(ior)));
	}

	float clearcoat = 0.0f;
	bool has_clearcoat_value = false;
	if (UsdShadeInput clearcoat_input = preview_surface.GetInput(TfToken("clearcoat"))) {
		has_clearcoat_value = clearcoat_input.Get(&clearcoat, p_time);
	}
	float clearcoat_roughness = 0.01f;
	bool has_clearcoat_roughness_value = false;
	if (UsdShadeInput clearcoat_roughness_input = preview_surface.GetInput(TfToken("clearcoatRoughness"))) {
		has_clearcoat_roughness_value = clearcoat_roughness_input.Get(&clearcoat_roughness, p_time);
	}

	UsdShadeInput clearcoat_input = preview_surface.GetInput(TfToken("clearcoat"));
	UsdShadeInput clearcoat_roughness_input = preview_surface.GetInput(TfToken("clearcoatRoughness"));
	const UsdShaderConnection clearcoat_texture_connection = clearcoat_input ? get_connected_texture_shader(p_stage, p_time, clearcoat_input) : UsdShaderConnection();
	const UsdShaderConnection clearcoat_roughness_texture_connection = clearcoat_roughness_input ? get_connected_texture_shader(p_stage, p_time, clearcoat_roughness_input) : UsdShaderConnection();

	const bool has_clearcoat = has_clearcoat_value || has_clearcoat_roughness_value || clearcoat_texture_connection || clearcoat_roughness_texture_connection;
	if (has_clearcoat) {
		material->set_feature(BaseMaterial3D::FEATURE_CLEARCOAT, true);
		if (has_clearcoat_value) {
			material->set_clearcoat(clearcoat);
		}
		if (has_clearcoat_roughness_value) {
			material->set_clearcoat_roughness(clearcoat_roughness);
		}
	}

	if (clearcoat_texture_connection || clearcoat_roughness_texture_connection) {
		if (clearcoat_texture_connection && clearcoat_roughness_texture_connection &&
				clearcoat_texture_connection.shader.GetPath() == clearcoat_roughness_texture_connection.shader.GetPath() &&
				is_texture_output_channel(clearcoat_texture_connection.output_name, BaseMaterial3D::TEXTURE_CHANNEL_RED) &&
				is_texture_output_channel(clearcoat_roughness_texture_connection.output_name, BaseMaterial3D::TEXTURE_CHANNEL_GREEN)) {
			record_preview_texture_source(p_stage, p_time, &texture_sources, "clearcoat", clearcoat_texture_connection);
			record_preview_texture_source(p_stage, p_time, &texture_sources, "clearcoatRoughness", clearcoat_roughness_texture_connection);
			Ref<Texture2D> texture = load_texture_from_shader(p_stage, p_time, clearcoat_texture_connection.shader);
			if (texture.is_valid()) {
				material->set_texture(BaseMaterial3D::TEXTURE_CLEARCOAT, texture);
				merge_material_uv_transform(p_stage, p_time, clearcoat_texture_connection.shader, material.ptr(), &has_uv_transform, &uv_scale, &uv_offset, r_mapping_notes);
			}
		} else {
			if (r_mapping_notes != nullptr) {
				(*r_mapping_notes)["usd:material_status"] = vformat("Material %s uses clearcoat texture wiring that cannot be represented by StandardMaterial3D; only scalar clearcoat values were applied.", material_path);
			}
			if (clearcoat_texture_connection) {
				record_preview_texture_source(p_stage, p_time, &texture_sources, "clearcoat", clearcoat_texture_connection);
			}
			if (clearcoat_roughness_texture_connection) {
				record_preview_texture_source(p_stage, p_time, &texture_sources, "clearcoatRoughness", clearcoat_roughness_texture_connection);
			}
		}
	}

	UsdShadeInput occlusion_input = preview_surface.GetInput(TfToken("occlusion"));
	if (occlusion_input && occlusion_input.HasConnectedSource()) {
		const UsdShaderConnection texture_connection = get_connected_texture_shader(p_stage, p_time, occlusion_input);
		if (texture_connection) {
			record_preview_texture_source(p_stage, p_time, &texture_sources, "occlusion", texture_connection);
			Ref<Texture2D> texture = load_texture_from_shader(p_stage, p_time, texture_connection.shader);
			if (texture.is_valid()) {
				material->set_feature(BaseMaterial3D::FEATURE_AMBIENT_OCCLUSION, true);
				material->set_texture(BaseMaterial3D::TEXTURE_AMBIENT_OCCLUSION, texture);
				material->set_ao_texture_channel(get_texture_channel_for_output(texture_connection.output_name));
				merge_material_uv_transform(p_stage, p_time, texture_connection.shader, material.ptr(), &has_uv_transform, &uv_scale, &uv_offset, r_mapping_notes);
			}
		} else if (r_mapping_notes != nullptr) {
			(*r_mapping_notes)["usd:material_status"] = vformat("Material %s uses an unsupported occlusion source shader.", material_path);
		}
	}

	if (!texture_sources.is_empty()) {
		set_usd_metadata(material.ptr(), "usd:preview_surface_texture_sources", texture_sources);
	}

	return material;
}

bool write_preview_material(const UsdStageRefPtr &p_stage, const Ref<Material> &p_material, const SdfPath &p_mesh_path, const String &p_save_path, const String &p_material_key, UsdShadeMaterial *r_material) {
	ERR_FAIL_NULL_V(r_material, false);
	*r_material = UsdShadeMaterial();
	ERR_FAIL_COND_V(p_stage == nullptr, false);

	BaseMaterial3D *base_material = Object::cast_to<BaseMaterial3D>(p_material.ptr());
	if (base_material == nullptr) {
		return false;
	}

	const String material_name_source = p_material_key.is_empty() ? (p_material->get_name().is_empty() ? String("Material") : String(p_material->get_name())) : p_material_key;
	const String material_name = make_valid_identifier(material_name_source);
	const SdfPath material_path = p_mesh_path.AppendChild(TfToken("Looks")).AppendChild(TfToken(material_name.utf8().get_data()));

	UsdShadeMaterial usd_material = UsdShadeMaterial::Define(p_stage, material_path);
	UsdShadeShader preview_surface = UsdShadeShader::Define(p_stage, material_path.AppendChild(TfToken("PreviewSurface")));
	preview_surface.CreateIdAttr(VtValue(TfToken("UsdPreviewSurface")));
	usd_material.CreateSurfaceOutput().ConnectToSource(preview_surface.CreateOutput(TfToken("surface"), SdfValueTypeNames->Token));

	const Color albedo = base_material->get_albedo();
	const Dictionary material_metadata = get_usd_metadata(base_material);
	const bool use_specular_workflow = (bool)material_metadata.get("usd:preview_surface_use_specular_workflow", false);
	const Variant ior_variant = material_metadata.get("usd:preview_surface_ior", Variant());

	preview_surface.CreateInput(TfToken("diffuseColor"), SdfValueTypeNames->Color3f).Set(GfVec3f(albedo.r, albedo.g, albedo.b));
	if (!use_specular_workflow) {
		preview_surface.CreateInput(TfToken("metallic"), SdfValueTypeNames->Float).Set(base_material->get_metallic());
	} else {
		preview_surface.CreateInput(TfToken("useSpecularWorkflow"), SdfValueTypeNames->Bool).Set(true);
		if (material_metadata.has("usd:preview_surface_specular_color")) {
			const Color specular_color = material_metadata["usd:preview_surface_specular_color"];
			preview_surface.CreateInput(TfToken("specularColor"), SdfValueTypeNames->Color3f).Set(GfVec3f(specular_color.r, specular_color.g, specular_color.b));
		} else {
			const float preview_f0 = preview_f0_from_godot_specular(base_material->get_specular());
			preview_surface.CreateInput(TfToken("specularColor"), SdfValueTypeNames->Color3f).Set(GfVec3f(preview_f0, preview_f0, preview_f0));
		}
	}
	preview_surface.CreateInput(TfToken("roughness"), SdfValueTypeNames->Float).Set(base_material->get_roughness());
	if (ior_variant.get_type() == Variant::FLOAT || ior_variant.get_type() == Variant::INT) {
		preview_surface.CreateInput(TfToken("ior"), SdfValueTypeNames->Float).Set((float)(double)ior_variant);
	}

	const bool has_emission = base_material->get_feature(BaseMaterial3D::FEATURE_EMISSION) ||
			base_material->get_texture(BaseMaterial3D::TEXTURE_EMISSION).is_valid() ||
			base_material->get_emission() != Color(0.0f, 0.0f, 0.0f, 1.0f);
	if (has_emission) {
		const Color emission = base_material->get_emission();
		preview_surface.CreateInput(TfToken("emissiveColor"), SdfValueTypeNames->Color3f).Set(GfVec3f(emission.r, emission.g, emission.b));
	}

	if (base_material->get_transparency() != BaseMaterial3D::TRANSPARENCY_DISABLED) {
		preview_surface.CreateInput(TfToken("opacity"), SdfValueTypeNames->Float).Set(CLAMP(albedo.a, 0.0f, 1.0f));
	}
	if (base_material->get_transparency() == BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR) {
		preview_surface.CreateInput(TfToken("opacityThreshold"), SdfValueTypeNames->Float).Set(CLAMP(base_material->get_alpha_scissor_threshold(), 0.0f, 1.0f));
	}

	const bool has_clearcoat = base_material->get_feature(BaseMaterial3D::FEATURE_CLEARCOAT) || !Math::is_zero_approx(base_material->get_clearcoat());
	if (has_clearcoat) {
		preview_surface.CreateInput(TfToken("clearcoat"), SdfValueTypeNames->Float).Set(base_material->get_clearcoat());
		preview_surface.CreateInput(TfToken("clearcoatRoughness"), SdfValueTypeNames->Float).Set(base_material->get_clearcoat_roughness());
	}

	connect_preview_texture(p_stage, p_save_path, base_material, base_material->get_texture(BaseMaterial3D::TEXTURE_ALBEDO), material_path, "AlbedoTexture", "diffuseColor", SdfValueTypeNames->Color3f, TfToken("rgb"), SdfValueTypeNames->Float3);
	connect_preview_texture(p_stage, p_save_path, base_material, base_material->get_texture(BaseMaterial3D::TEXTURE_EMISSION), material_path, "EmissionTexture", "emissiveColor", SdfValueTypeNames->Color3f, TfToken("rgb"), SdfValueTypeNames->Float3);
	connect_preview_texture(p_stage, p_save_path, base_material, base_material->get_texture(BaseMaterial3D::TEXTURE_NORMAL), material_path, "NormalTexture", "normal", SdfValueTypeNames->Normal3f, TfToken("rgb"), SdfValueTypeNames->Float3);
	connect_preview_texture(p_stage, p_save_path, base_material, base_material->get_texture(BaseMaterial3D::TEXTURE_METALLIC), material_path, "MetallicTexture", "metallic", SdfValueTypeNames->Float, get_usd_texture_output_for_channel(base_material->get_metallic_texture_channel()), get_usd_output_type_for_channel(base_material->get_metallic_texture_channel()));
	connect_preview_texture(p_stage, p_save_path, base_material, base_material->get_texture(BaseMaterial3D::TEXTURE_ROUGHNESS), material_path, "RoughnessTexture", "roughness", SdfValueTypeNames->Float, get_usd_texture_output_for_channel(base_material->get_roughness_texture_channel()), get_usd_output_type_for_channel(base_material->get_roughness_texture_channel()));
	if (base_material->get_transparency() != BaseMaterial3D::TRANSPARENCY_DISABLED) {
		const Dictionary opacity_source = get_preview_surface_texture_source(base_material, "opacity");
		if (!opacity_source.is_empty()) {
			const String source_asset_path = opacity_source.get("asset_path", String());
			const String output_name = opacity_source.get("output_name", String("a"));
			connect_preview_texture_asset_path(p_stage, source_asset_path, base_material, material_path, "OpacityTexture", "opacity", SdfValueTypeNames->Float, get_usd_texture_output_for_name(output_name), get_usd_output_type_for_name(output_name));
		} else {
			connect_preview_texture(p_stage, p_save_path, base_material, base_material->get_texture(BaseMaterial3D::TEXTURE_ALBEDO), material_path, "AlbedoTexture", "opacity", SdfValueTypeNames->Float, TfToken("a"), SdfValueTypeNames->Float);
		}
	}
	if (has_clearcoat) {
		const Ref<Texture2D> clearcoat_texture = base_material->get_texture(BaseMaterial3D::TEXTURE_CLEARCOAT);
		connect_preview_texture(p_stage, p_save_path, base_material, clearcoat_texture, material_path, "ClearcoatTexture", "clearcoat", SdfValueTypeNames->Float, TfToken("r"), SdfValueTypeNames->Float);
		connect_preview_texture(p_stage, p_save_path, base_material, clearcoat_texture, material_path, "ClearcoatTexture", "clearcoatRoughness", SdfValueTypeNames->Float, TfToken("g"), SdfValueTypeNames->Float);
	}
	const bool has_occlusion = base_material->get_feature(BaseMaterial3D::FEATURE_AMBIENT_OCCLUSION) || base_material->get_texture(BaseMaterial3D::TEXTURE_AMBIENT_OCCLUSION).is_valid();
	if (has_occlusion) {
		connect_preview_texture(p_stage, p_save_path, base_material, base_material->get_texture(BaseMaterial3D::TEXTURE_AMBIENT_OCCLUSION), material_path, "OcclusionTexture", "occlusion", SdfValueTypeNames->Float, get_usd_texture_output_for_channel(base_material->get_ao_texture_channel()), get_usd_output_type_for_channel(base_material->get_ao_texture_channel()));
	}

	*r_material = usd_material;
	return true;
}

Ref<Material> make_display_color_material(const Color &p_color, bool p_use_vertex_colors) {
	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_albedo(p_color);
	if (p_use_vertex_colors) {
		material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		material->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
	}
	return material;
}

} // namespace godot_usd
