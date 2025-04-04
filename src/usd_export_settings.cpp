#include "usd_export_settings.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/node.hpp>

namespace godot {

void UsdExportSettings::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_export_materials", "enabled"), &UsdExportSettings::set_export_materials);
    ClassDB::bind_method(D_METHOD("get_export_materials"), &UsdExportSettings::get_export_materials);
    
    ClassDB::bind_method(D_METHOD("set_export_animations", "enabled"), &UsdExportSettings::set_export_animations);
    ClassDB::bind_method(D_METHOD("get_export_animations"), &UsdExportSettings::get_export_animations);
    
    ClassDB::bind_method(D_METHOD("set_export_cameras", "enabled"), &UsdExportSettings::set_export_cameras);
    ClassDB::bind_method(D_METHOD("get_export_cameras"), &UsdExportSettings::get_export_cameras);
    
    ClassDB::bind_method(D_METHOD("set_export_lights", "enabled"), &UsdExportSettings::set_export_lights);
    ClassDB::bind_method(D_METHOD("get_export_lights"), &UsdExportSettings::get_export_lights);
    
    ClassDB::bind_method(D_METHOD("set_export_meshes", "enabled"), &UsdExportSettings::set_export_meshes);
    ClassDB::bind_method(D_METHOD("get_export_meshes"), &UsdExportSettings::get_export_meshes);
    
    ClassDB::bind_method(D_METHOD("set_export_textures", "enabled"), &UsdExportSettings::set_export_textures);
    ClassDB::bind_method(D_METHOD("get_export_textures"), &UsdExportSettings::get_export_textures);
    
    ClassDB::bind_method(D_METHOD("set_copyright", "copyright"), &UsdExportSettings::set_copyright);
    ClassDB::bind_method(D_METHOD("get_copyright"), &UsdExportSettings::get_copyright);
    
    ClassDB::bind_method(D_METHOD("set_bake_fps", "fps"), &UsdExportSettings::set_bake_fps);
    ClassDB::bind_method(D_METHOD("get_bake_fps"), &UsdExportSettings::get_bake_fps);
    
    ClassDB::bind_method(D_METHOD("set_use_binary_format", "enabled"), &UsdExportSettings::set_use_binary_format);
    ClassDB::bind_method(D_METHOD("get_use_binary_format"), &UsdExportSettings::get_use_binary_format);
    
    ClassDB::bind_method(D_METHOD("set_flatten_stage", "enabled"), &UsdExportSettings::set_flatten_stage);
    ClassDB::bind_method(D_METHOD("get_flatten_stage"), &UsdExportSettings::get_flatten_stage);
    
    ClassDB::bind_method(D_METHOD("set_export_as_single_layer", "enabled"), &UsdExportSettings::set_export_as_single_layer);
    ClassDB::bind_method(D_METHOD("get_export_as_single_layer"), &UsdExportSettings::get_export_as_single_layer);
    
    ClassDB::bind_method(D_METHOD("set_export_with_references", "enabled"), &UsdExportSettings::set_export_with_references);
    ClassDB::bind_method(D_METHOD("get_export_with_references"), &UsdExportSettings::get_export_with_references);
    
    // Add properties to the inspector
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "export_materials", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_export_materials", "get_export_materials");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "export_animations", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_export_animations", "get_export_animations");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "export_cameras", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_export_cameras", "get_export_cameras");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "export_lights", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_export_lights", "get_export_lights");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "export_meshes", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_export_meshes", "get_export_meshes");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "export_textures", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_export_textures", "get_export_textures");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "copyright", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_copyright", "get_copyright");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bake_fps", PROPERTY_HINT_RANGE, "1,120,0.1"), "set_bake_fps", "get_bake_fps");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_binary_format", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_use_binary_format", "get_use_binary_format");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flatten_stage", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_flatten_stage", "get_flatten_stage");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "export_as_single_layer", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_export_as_single_layer", "get_export_as_single_layer");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "export_with_references", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_export_with_references", "get_export_with_references");
}

UsdExportSettings::UsdExportSettings() {
    // Set default values
    _export_materials = true;
    _export_animations = true;
    _export_cameras = true;
    _export_lights = true;
    _export_meshes = true;
    _export_textures = true;
    _copyright = "";
    _bake_fps = 30.0f;
    
    _use_binary_format = false;
    _flatten_stage = false;
    _export_as_single_layer = true;
    _export_with_references = false;
}

void UsdExportSettings::set_export_materials(bool p_enabled) {
    _export_materials = p_enabled;
}

bool UsdExportSettings::get_export_materials() const {
    return _export_materials;
}

void UsdExportSettings::set_export_animations(bool p_enabled) {
    _export_animations = p_enabled;
}

bool UsdExportSettings::get_export_animations() const {
    return _export_animations;
}

void UsdExportSettings::set_export_cameras(bool p_enabled) {
    _export_cameras = p_enabled;
}

bool UsdExportSettings::get_export_cameras() const {
    return _export_cameras;
}

void UsdExportSettings::set_export_lights(bool p_enabled) {
    _export_lights = p_enabled;
}

bool UsdExportSettings::get_export_lights() const {
    return _export_lights;
}

void UsdExportSettings::set_export_meshes(bool p_enabled) {
    _export_meshes = p_enabled;
}

bool UsdExportSettings::get_export_meshes() const {
    return _export_meshes;
}

void UsdExportSettings::set_export_textures(bool p_enabled) {
    _export_textures = p_enabled;
}

bool UsdExportSettings::get_export_textures() const {
    return _export_textures;
}

void UsdExportSettings::set_copyright(const String &p_copyright) {
    _copyright = p_copyright;
}

String UsdExportSettings::get_copyright() const {
    return _copyright;
}

void UsdExportSettings::set_bake_fps(float p_fps) {
    _bake_fps = p_fps;
}

float UsdExportSettings::get_bake_fps() const {
    return _bake_fps;
}

void UsdExportSettings::set_use_binary_format(bool p_enabled) {
    _use_binary_format = p_enabled;
}

bool UsdExportSettings::get_use_binary_format() const {
    return _use_binary_format;
}

void UsdExportSettings::set_flatten_stage(bool p_enabled) {
    _flatten_stage = p_enabled;
}

bool UsdExportSettings::get_flatten_stage() const {
    return _flatten_stage;
}

void UsdExportSettings::set_export_as_single_layer(bool p_enabled) {
    _export_as_single_layer = p_enabled;
}

bool UsdExportSettings::get_export_as_single_layer() const {
    return _export_as_single_layer;
}

void UsdExportSettings::set_export_with_references(bool p_enabled) {
    _export_with_references = p_enabled;
}

bool UsdExportSettings::get_export_with_references() const {
    return _export_with_references;
}

void UsdExportSettings::generate_property_list(Ref<UsdDocument> p_document, Node *p_root) {
    // This method would be used to dynamically generate properties based on the scene
    // For now, we'll just use the static properties defined in _bind_methods
    
    // In a more advanced implementation, we might analyze the scene to determine
    // what types of nodes are present and adjust the available options accordingly
    
    if (p_root) {
        UtilityFunctions::print("Generating property list for scene: ", p_root->get_name());
    }
}

}
