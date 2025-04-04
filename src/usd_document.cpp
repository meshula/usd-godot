#include "usd_document.h"
#include "usd_state.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/light3d.hpp>
#include <godot_cpp/classes/animation_player.hpp>

// USD headers
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/sdf/copyUtils.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace godot {

void UsdDocument::_bind_methods() {
    ClassDB::bind_method(D_METHOD("append_from_scene", "scene_root", "state", "flags"), &UsdDocument::append_from_scene, DEFVAL(0));
    ClassDB::bind_method(D_METHOD("write_to_filesystem", "state", "path"), &UsdDocument::write_to_filesystem);
    ClassDB::bind_method(D_METHOD("get_file_extension_for_format", "binary"), &UsdDocument::get_file_extension_for_format);
}

UsdDocument::UsdDocument() {
    // Initialize USD-specific members
    // This will be expanded as we implement the USD integration
}

Error UsdDocument::append_from_scene(Node *p_scene_root, Ref<UsdState> p_state, int32_t p_flags) {
    // This method will convert the Godot scene to USD
    // For now, we'll just print some debug information
    
    if (!p_scene_root) {
        UtilityFunctions::printerr("USD Export: No scene root provided");
        return ERR_INVALID_PARAMETER;
    }
    
    if (p_state.is_null()) {
        UtilityFunctions::printerr("USD Export: No state provided");
        return ERR_INVALID_PARAMETER;
    }
    
    UtilityFunctions::print("USD Export: Appending scene ", p_scene_root->get_name(), " to USD document");
    
    // In a real implementation, we would:
    // 1. Create a USD stage
    // 2. Create a root prim for the scene
    // 3. Traverse the Godot scene tree
    // 4. Convert each node to a USD prim
    // 5. Set properties on the prims
    
    // For now, we'll just traverse the scene and print the node types
    _traverse_scene(p_scene_root, 0);
    
    return OK;
}

void UsdDocument::_traverse_scene(Node *p_node, int p_depth) {
    if (!p_node) {
        return;
    }
    
    // Print indentation based on depth
    String indent = "";
    for (int i = 0; i < p_depth; i++) {
        indent += "  ";
    }
    
    // Print node information
    String node_type = p_node->get_class();
    String node_name = p_node->get_name();
    UtilityFunctions::print(indent, "Node: ", node_name, " (", node_type, ")");
    
    // Process specific node types
    if (Object::cast_to<Node3D>(p_node)) {
        Node3D *node_3d = Object::cast_to<Node3D>(p_node);
        UtilityFunctions::print(indent, "  Transform: ", node_3d->get_transform());
    }
    
    if (Object::cast_to<MeshInstance3D>(p_node)) {
        MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(p_node);
        UtilityFunctions::print(indent, "  Mesh: ", mesh_instance->get_mesh());
    }
    
    if (Object::cast_to<Camera3D>(p_node)) {
        Camera3D *camera = Object::cast_to<Camera3D>(p_node);
        UtilityFunctions::print(indent, "  Camera FOV: ", camera->get_fov());
    }
    
    if (Object::cast_to<Light3D>(p_node)) {
        Light3D *light = Object::cast_to<Light3D>(p_node);
        UtilityFunctions::print(indent, "  Light Energy: ", light->get_param(Light3D::PARAM_ENERGY));
    }
    
    // Recursively process children
    for (int i = 0; i < p_node->get_child_count(); i++) {
        _traverse_scene(p_node->get_child(i), p_depth + 1);
    }
}

Error UsdDocument::write_to_filesystem(Ref<UsdState> p_state, const String &p_path) {
    // This method will write the USD document to a file using the USD API
    
    if (p_state.is_null()) {
        UtilityFunctions::printerr("USD Export: No state provided");
        return ERR_INVALID_PARAMETER;
    }
    
    if (p_path.is_empty()) {
        UtilityFunctions::printerr("USD Export: No file path provided");
        return ERR_INVALID_PARAMETER;
    }
    
    UtilityFunctions::print("USD Export: Writing USD document to ", p_path);
    
    try {
        // Create a new USD stage
        UsdStageRefPtr stage = UsdStage::CreateNew(p_path.utf8().get_data());
        if (!stage) {
            UtilityFunctions::printerr("USD Export: Failed to create USD stage");
            return ERR_CANT_CREATE;
        }
        
        // Set up the stage
        stage->SetStartTimeCode(1);
        stage->SetEndTimeCode(1);
        stage->SetTimeCodesPerSecond(p_state->get_bake_fps());
        
        // Create a default prim
        UsdGeomXform root = UsdGeomXform::Define(stage, SdfPath("/Root"));
        stage->SetDefaultPrim(root.GetPrim());
        
        // Add metadata (using custom layer data for copyright)
        if (!p_state->get_copyright().is_empty()) {
            auto rootLayer = stage->GetRootLayer();
            auto customData = rootLayer->GetCustomLayerData();
            customData["copyright"] = VtValue(std::string(p_state->get_copyright().utf8().get_data()));
            rootLayer->SetCustomLayerData(customData);
        }
        
        // Create a cube
        UsdGeomCube cube = UsdGeomCube::Define(stage, SdfPath("/Root/Cube"));
        
        // Set cube size (default is 2 units, so we'll set it to 1 unit)
        cube.GetSizeAttr().Set(1.0);
        
        // Save the stage
        stage->Save();
        
        UtilityFunctions::print("USD Export: Successfully exported scene to ", p_path);
        return OK;
    } catch (const std::exception& e) {
        UtilityFunctions::printerr("USD Export: Exception occurred: ", e.what());
        return ERR_CANT_CREATE;
    }
}

String UsdDocument::get_file_extension_for_format(bool p_binary) const {
    // Return the appropriate file extension based on the format
    return p_binary ? "usdc" : "usda";
}

}
