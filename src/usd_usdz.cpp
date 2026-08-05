// ---------------------------------------------------------------------------------------------
// Transplanted from godot-usd (https://github.com/migueldeicaza) — MIT License, (c) 2026 Miguel de
// Icaza. Harvested into usd-godot per USD_GODOT_INTEGRATION_PLAN.md (Phase 2), lifted near-verbatim
// and kept in the godot_usd namespace; adapted only as needed for Godot 4.7 / usd-godot. See
// THIRD_PARTY.md for provenance.
// ---------------------------------------------------------------------------------------------

#include "usd_usdz.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/zip_reader.hpp>
#include <godot_cpp/core/error_macros.hpp>

#include "usd_stage_utils.h"

#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/zipFile.h>

namespace godot_usd {

using namespace godot;
using namespace pxr;

String normalize_usd_asset_path(const String &p_asset_path) {
	return p_asset_path.replace("\\", "/");
}

bool split_usdz_package_asset_path(const String &p_asset_path, String *r_package_path, String *r_member_path) {
	ERR_FAIL_NULL_V(r_package_path, false);
	ERR_FAIL_NULL_V(r_member_path, false);

	const int package_delimiter = p_asset_path.rfind("[");
	if (package_delimiter == -1 || !p_asset_path.ends_with("]")) {
		return false;
	}

	*r_package_path = p_asset_path.substr(0, package_delimiter);
	*r_member_path = p_asset_path.substr(package_delimiter + 1, p_asset_path.length() - package_delimiter - 2);
	return !r_package_path->is_empty() && !r_member_path->is_empty();
}

String get_asset_extension(const String &p_asset_path) {
	String extension_path = normalize_usd_asset_path(p_asset_path);
	const int package_delimiter = extension_path.rfind("[");
	if (package_delimiter != -1) {
		extension_path = extension_path.substr(package_delimiter + 1, extension_path.length() - package_delimiter - 1);
	}
	if (extension_path.ends_with("]")) {
		extension_path = extension_path.left(extension_path.length() - 1);
	}
	return extension_path.get_extension().to_lower();
}

bool read_usdz_package_member_bytes(const String &p_package_path, const String &p_member_path, PackedByteArray *r_bytes) {
	ERR_FAIL_NULL_V(r_bytes, false);
	r_bytes->clear();

	Ref<ZIPReader> zip_reader;
	zip_reader.instantiate();
	if (zip_reader->open(p_package_path) != OK) {
		return false;
	}

	const String member_path = p_member_path.replace("\\", "/");
	if (!zip_reader->file_exists(member_path, true) && !zip_reader->file_exists(member_path, false)) {
		zip_reader->close();
		return false;
	}

	*r_bytes = zip_reader->read_file(member_path, zip_reader->file_exists(member_path, true));
	zip_reader->close();
	return !r_bytes->is_empty();
}

Ref<Image> load_image_from_asset_bytes(const PackedByteArray &p_bytes, const String &p_asset_path) {
	if (p_bytes.is_empty()) {
		return Ref<Image>();
	}

	Ref<Image> image;
	image.instantiate();

	const String extension = get_asset_extension(p_asset_path);
	Error err = ERR_FILE_UNRECOGNIZED;
	if (extension == "png") {
		err = image->load_png_from_buffer(p_bytes);
	} else if (extension == "jpg" || extension == "jpeg") {
		err = image->load_jpg_from_buffer(p_bytes);
	} else if (extension == "webp") {
		err = image->load_webp_from_buffer(p_bytes);
	} else if (extension == "tga") {
		err = image->load_tga_from_buffer(p_bytes);
	} else if (extension == "bmp") {
		err = image->load_bmp_from_buffer(p_bytes);
	} else if (extension == "ktx") {
		err = image->load_ktx_from_buffer(p_bytes);
	} else if (extension == "svg") {
		err = image->load_svg_from_buffer(p_bytes);
	}

	if (err != OK) {
		return Ref<Image>();
	}

	return image;
}

String resolve_asset_path(const UsdStageRefPtr &p_stage, const SdfAssetPath &p_asset_path) {
	ERR_FAIL_COND_V(p_stage == nullptr, String());

	const String resolved_path = normalize_usd_asset_path(to_godot_string(p_asset_path.GetResolvedPath()));
	if (!resolved_path.is_empty()) {
		return resolved_path;
	}

	const String authored_path = normalize_usd_asset_path(to_godot_string(p_asset_path.GetAssetPath()));
	if (authored_path.is_empty()) {
		return String();
	}
	if (authored_path.is_absolute_path()) {
		return authored_path;
	}

	String layer_path = normalize_usd_asset_path(to_godot_string(p_stage->GetRootLayer()->GetRealPath()));
	if (layer_path.is_empty()) {
		layer_path = normalize_usd_asset_path(to_godot_string(p_stage->GetRootLayer()->GetIdentifier()));
	}
	if (!layer_path.is_empty()) {
		if (layer_path.contains("[") && layer_path.ends_with("]")) {
			const int package_delimiter = layer_path.rfind("[");
			const String package_path = layer_path.substr(0, package_delimiter);
			const String member_path = layer_path.substr(package_delimiter + 1, layer_path.length() - package_delimiter - 2).get_base_dir();
			return package_path + String("[") + member_path.path_join(authored_path) + String("]");
		}
		return layer_path.get_base_dir().path_join(authored_path);
	}

	return authored_path;
}

Ref<Image> load_image_from_asset_attribute(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdAttribute &p_asset_attribute, String *r_resolved_path) {
	if (!p_asset_attribute) {
		return Ref<Image>();
	}

	SdfAssetPath asset_path;
	if (!p_asset_attribute.Get(&asset_path, p_time)) {
		return Ref<Image>();
	}

	const String resolved_path = resolve_asset_path(p_stage, asset_path);
	if (resolved_path.is_empty()) {
		return Ref<Image>();
	}

	if (r_resolved_path != nullptr) {
		*r_resolved_path = resolved_path;
	}

	String package_path;
	String package_member_path;
	if (split_usdz_package_asset_path(resolved_path, &package_path, &package_member_path)) {
		PackedByteArray asset_bytes;
		if (!read_usdz_package_member_bytes(package_path, package_member_path, &asset_bytes)) {
			return Ref<Image>();
		}
		return load_image_from_asset_bytes(asset_bytes, package_member_path);
	}

	return Image::load_from_file(resolved_path);
}

Ref<Texture2D> texture_from_image(const Ref<Image> &p_image) {
	if (p_image.is_null()) {
		return Ref<Texture2D>();
	}
	return ImageTexture::create_from_image(p_image);
}

Ref<Texture2D> load_texture_from_asset_attribute(const UsdStageRefPtr &p_stage, const UsdTimeCode &p_time, const UsdAttribute &p_asset_attribute, String *r_resolved_path) {
	return texture_from_image(load_image_from_asset_attribute(p_stage, p_time, p_asset_attribute, r_resolved_path));
}

bool is_safe_usdz_member_path(const String &p_member_path) {
	const String normalized_path = p_member_path.replace("\\", "/");
	if (normalized_path.is_empty() || normalized_path.is_absolute_path() || normalized_path.contains(":")) {
		return false;
	}

	const PackedStringArray path_parts = normalized_path.split("/", false);
	for (int i = 0; i < path_parts.size(); i++) {
		if (path_parts[i] == "." || path_parts[i] == "..") {
			return false;
		}
	}

	return true;
}

Error extract_usdz_package(const String &p_source_absolute_path, const String &p_destination_directory, String *r_root_layer_path, Vector<String> *r_package_file_paths) {
	ERR_FAIL_NULL_V(r_root_layer_path, ERR_INVALID_PARAMETER);
	ERR_FAIL_NULL_V(r_package_file_paths, ERR_INVALID_PARAMETER);
	*r_root_layer_path = String();
	r_package_file_paths->clear();

	Ref<ZIPReader> zip_reader;
	zip_reader.instantiate();
	const Error open_error = zip_reader->open(p_source_absolute_path);
	ERR_FAIL_COND_V_MSG(open_error != OK, open_error, vformat("Failed to open source USDZ package: %s", p_source_absolute_path));

	const PackedStringArray files = zip_reader->get_files();
	for (int i = 0; i < files.size(); i++) {
		String member_path = files[i].replace("\\", "/");
		if (!is_safe_usdz_member_path(member_path)) {
			zip_reader->close();
			ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, vformat("Unsafe path in USDZ package: %s", member_path));
		}

		const bool compressed = zip_reader->file_exists(member_path, true);
		const PackedByteArray bytes = zip_reader->read_file(member_path, compressed);
		const String destination_path = p_destination_directory.path_join(member_path);
		const Error mkdir_error = DirAccess::make_dir_recursive_absolute(destination_path.get_base_dir());
		if (mkdir_error != OK) {
			zip_reader->close();
			return mkdir_error;
		}

		Ref<FileAccess> destination_file = FileAccess::open(destination_path, FileAccess::WRITE);
		if (destination_file.is_null()) {
			zip_reader->close();
			return ERR_CANT_CREATE;
		}
		destination_file->store_buffer(bytes);
		if (destination_file->get_error() != OK) {
			zip_reader->close();
			return destination_file->get_error();
		}

		if (r_root_layer_path->is_empty()) {
			const String extension = member_path.get_extension().to_lower();
			if (extension == "usd" || extension == "usda" || extension == "usdc") {
				*r_root_layer_path = member_path;
			}
		}
		r_package_file_paths->push_back(member_path);
	}

	zip_reader->close();
	ERR_FAIL_COND_V_MSG(r_root_layer_path->is_empty(), ERR_FILE_CORRUPT, vformat("USDZ package has no root layer: %s", p_source_absolute_path));
	return OK;
}

Error create_usdz_package_from_extracted_files(const String &p_extracted_directory, const Vector<String> &p_package_file_paths, const String &p_root_layer_path, const String &p_package_path) {
	ERR_FAIL_COND_V_MSG(p_package_file_paths.is_empty(), ERR_INVALID_PARAMETER, "USDZ package cannot be created without extracted package files.");

	SdfZipFileWriter package_writer = SdfZipFileWriter::CreateNew(p_package_path.utf8().get_data());
	ERR_FAIL_COND_V_MSG(!package_writer, ERR_CANT_CREATE, vformat("Failed to create USDZ package writer: %s", p_package_path));

	const String root_layer_absolute_path = p_extracted_directory.path_join(p_root_layer_path);
	if (package_writer.AddFile(root_layer_absolute_path.utf8().get_data(), p_root_layer_path.utf8().get_data()).empty()) {
		package_writer.Discard();
		ERR_FAIL_V_MSG(ERR_CANT_CREATE, vformat("Failed to add USDZ root layer to package: %s", p_root_layer_path));
	}

	for (int i = 0; i < p_package_file_paths.size(); i++) {
		const String package_file_path = p_package_file_paths[i];
		if (package_file_path == p_root_layer_path) {
			continue;
		}

		const String extracted_file_path = p_extracted_directory.path_join(package_file_path);
		if (package_writer.AddFile(extracted_file_path.utf8().get_data(), package_file_path.utf8().get_data()).empty()) {
			package_writer.Discard();
			ERR_FAIL_V_MSG(ERR_CANT_CREATE, vformat("Failed to add USDZ package member: %s", package_file_path));
		}
	}

	return package_writer.Save() ? OK : ERR_CANT_CREATE;
}

} // namespace godot_usd
