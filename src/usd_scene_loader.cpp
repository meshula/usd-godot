#include "usd_scene_loader.h"
#include "usd_scene_builder.h"
#include "usd_stage_utils.h"   // set_usd_metadata

#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

// USD headers
#include <pxr/usd/usd/stage.h>

namespace godot {

PackedStringArray UsdResourceFormatLoader::_get_recognized_extensions() const {
    PackedStringArray exts;
    exts.push_back("usd");
    exts.push_back("usda");
    exts.push_back("usdc");
    exts.push_back("usdz");
    return exts;
}

bool UsdResourceFormatLoader::_handles_type(const StringName &p_type) const {
    return p_type == StringName("PackedScene");
}

String UsdResourceFormatLoader::_get_resource_type(const String &p_path) const {
    const String ext = p_path.get_extension().to_lower();
    if (ext == "usd" || ext == "usda" || ext == "usdc" || ext == "usdz") {
        return "PackedScene";
    }
    return "";
}

Variant UsdResourceFormatLoader::_load(const String &p_path, const String &p_original_path, bool p_use_sub_threads, int32_t p_cache_mode) const {
    String abs_path = p_path;
    if (p_path.begins_with("res://") || p_path.begins_with("user://")) {
        abs_path = ProjectSettings::get_singleton()->globalize_path(p_path);
    }

    pxr::UsdStageRefPtr stage;
    try {
        stage = pxr::UsdStage::Open(abs_path.utf8().get_data());
    } catch (const std::exception &e) {
        UtilityFunctions::printerr("USD load: exception opening ", p_path, ": ", e.what());
        return Variant();
    }
    if (!stage) {
        UtilityFunctions::printerr("USD load: failed to open stage: ", p_path);
        return Variant();
    }

    Node *root = UsdSceneBuilder::build_scene(stage);
    if (!root) {
        UtilityFunctions::printerr("USD load: builder produced no root for ", p_path);
        return Variant();
    }

    // Stamp the originating USD path on the scene root: forensic provenance, and the anchor the
    // source-preserving saver uses to reopen the original layer and author only changed edits.
    godot_usd::set_usd_metadata(root, "usd:source_path", p_original_path.is_empty() ? p_path : p_original_path);

    Ref<PackedScene> packed;
    packed.instantiate();
    const Error pack_err = packed->pack(root);
    // The packed scene captures a snapshot of the tree; the live nodes are no longer needed.
    memdelete(root);

    if (pack_err != OK) {
        UtilityFunctions::printerr("USD load: PackedScene::pack failed for ", p_path);
        return Variant();
    }
    return packed;
}

} // namespace godot
