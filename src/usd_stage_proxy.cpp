#include "usd_stage_proxy.h"
#include "usd_prim_proxy.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>

// USD headers
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/sdf/layer.h>

using namespace usd_godot;

namespace godot {

void UsdStageProxy::_bind_methods() {
    // Stage Lifecycle
    ClassDB::bind_method(D_METHOD("open", "path"), &UsdStageProxy::open);
    ClassDB::bind_method(D_METHOD("create_new", "path"), &UsdStageProxy::create_new);
    ClassDB::bind_method(D_METHOD("save", "path"), &UsdStageProxy::save, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("export_to", "path", "binary"), &UsdStageProxy::export_to, DEFVAL(true));
    ClassDB::bind_method(D_METHOD("close"), &UsdStageProxy::close);
    ClassDB::bind_method(D_METHOD("reload"), &UsdStageProxy::reload);
    ClassDB::bind_method(D_METHOD("is_open"), &UsdStageProxy::is_open);
    ClassDB::bind_method(D_METHOD("is_modified"), &UsdStageProxy::is_modified);

    // Prim Access
    ClassDB::bind_method(D_METHOD("get_default_prim"), &UsdStageProxy::get_default_prim);
    ClassDB::bind_method(D_METHOD("set_default_prim", "prim_path"), &UsdStageProxy::set_default_prim);
    ClassDB::bind_method(D_METHOD("get_prim_at_path", "path"), &UsdStageProxy::get_prim_at_path);
    ClassDB::bind_method(D_METHOD("has_prim_at_path", "path"), &UsdStageProxy::has_prim_at_path);
    ClassDB::bind_method(D_METHOD("traverse"), &UsdStageProxy::traverse);
    ClassDB::bind_method(D_METHOD("traverse_by_type", "type_name"), &UsdStageProxy::traverse_by_type);

    // Prim Creation
    ClassDB::bind_method(D_METHOD("define_prim", "path", "type_name"), &UsdStageProxy::define_prim);
    ClassDB::bind_method(D_METHOD("remove_prim", "path"), &UsdStageProxy::remove_prim);

    // Prim Attributes and Transforms
    ClassDB::bind_method(D_METHOD("set_prim_attribute", "prim_path", "attr_name", "value_type", "value"),
                         &UsdStageProxy::set_prim_attribute);
    ClassDB::bind_method(D_METHOD("get_prim_attribute", "prim_path", "attr_name"),
                         &UsdStageProxy::get_prim_attribute);
    ClassDB::bind_method(D_METHOD("set_prim_transform", "prim_path", "tx", "ty", "tz", "rx", "ry", "rz", "sx", "sy", "sz"),
                         &UsdStageProxy::set_prim_transform);
    ClassDB::bind_method(D_METHOD("list_prims"), &UsdStageProxy::list_prims);

    // Shared State (MCP Interop)
    ClassDB::bind_method(D_METHOD("get_stage_id"), &UsdStageProxy::get_stage_id);
    ClassDB::bind_method(D_METHOD("get_generation"), &UsdStageProxy::get_generation);

    // Time / Animation
    ClassDB::bind_method(D_METHOD("set_time_code", "time"), &UsdStageProxy::set_time_code);
    ClassDB::bind_method(D_METHOD("get_time_code"), &UsdStageProxy::get_time_code);
    ClassDB::bind_method(D_METHOD("get_start_time_code"), &UsdStageProxy::get_start_time_code);
    ClassDB::bind_method(D_METHOD("get_end_time_code"), &UsdStageProxy::get_end_time_code);
    ClassDB::bind_method(D_METHOD("set_time_range", "start", "end"), &UsdStageProxy::set_time_range);
    ClassDB::bind_method(D_METHOD("get_frames_per_second"), &UsdStageProxy::get_frames_per_second);
    ClassDB::bind_method(D_METHOD("set_frames_per_second", "fps"), &UsdStageProxy::set_frames_per_second);

    // Stage Metadata
    ClassDB::bind_method(D_METHOD("get_root_layer_path"), &UsdStageProxy::get_root_layer_path);
    ClassDB::bind_method(D_METHOD("get_up_axis"), &UsdStageProxy::get_up_axis);
    ClassDB::bind_method(D_METHOD("set_up_axis", "axis"), &UsdStageProxy::set_up_axis);
    ClassDB::bind_method(D_METHOD("get_meters_per_unit"), &UsdStageProxy::get_meters_per_unit);
    ClassDB::bind_method(D_METHOD("set_meters_per_unit", "meters_per_unit"), &UsdStageProxy::set_meters_per_unit);

    // Layer Management
    ClassDB::bind_method(D_METHOD("get_sublayer_paths"), &UsdStageProxy::get_sublayer_paths);
    ClassDB::bind_method(D_METHOD("add_sublayer", "path"), &UsdStageProxy::add_sublayer);
    ClassDB::bind_method(D_METHOD("remove_sublayer", "path"), &UsdStageProxy::remove_sublayer);
}

UsdStageProxy::UsdStageProxy() : _stage_id(0), _current_time_code(0.0) {
}

UsdStageProxy::~UsdStageProxy() {
    close();
}

// -----------------------------------------------------------------------------
// Stage Lifecycle
// -----------------------------------------------------------------------------

Error UsdStageProxy::open(const String &p_path) {
    // Close any existing stage
    close();

    // Convert Godot path to filesystem path
    String abs_path = p_path;
    if (p_path.begins_with("res://") || p_path.begins_with("user://")) {
        abs_path = ProjectSettings::get_singleton()->globalize_path(p_path);
    }

    // Use UsdStageManager to open the stage
    _stage_id = UsdStageManager::get_singleton().open_stage(abs_path.utf8().get_data());

    if (_stage_id == 0) {
        UtilityFunctions::printerr("UsdStageProxy: Failed to open stage at ", p_path);
        return ERR_CANT_OPEN;
    }

    _file_path = p_path;
    return OK;
}

Error UsdStageProxy::create_new(const String &p_path) {
    // Close any existing stage
    close();

    // Convert Godot path to filesystem path
    String abs_path = p_path;
    if (p_path.begins_with("res://") || p_path.begins_with("user://")) {
        abs_path = ProjectSettings::get_singleton()->globalize_path(p_path);
    }

    // Use UsdStageManager to create the stage
    _stage_id = UsdStageManager::get_singleton().create_stage(abs_path.utf8().get_data());

    if (_stage_id == 0) {
        UtilityFunctions::printerr("UsdStageProxy: Failed to create stage at ", p_path);
        return ERR_CANT_CREATE;
    }

    _file_path = p_path;

    // Set up default stage metadata
    StageRecord* record = get_stage_record();
    if (record && record->get_stage()) {
        pxr::UsdGeomSetStageUpAxis(record->get_stage(), pxr::UsdGeomTokens->y);
        pxr::UsdGeomSetStageMetersPerUnit(record->get_stage(), 1.0);
    }

    return OK;
}

Error UsdStageProxy::save(const String &p_path) {
    if (_stage_id == 0) {
        UtilityFunctions::printerr("UsdStageProxy: No stage open");
        return ERR_UNCONFIGURED;
    }

    // Convert Godot path to filesystem path if needed
    String abs_path = p_path;
    if (!p_path.is_empty() && (p_path.begins_with("res://") || p_path.begins_with("user://"))) {
        abs_path = ProjectSettings::get_singleton()->globalize_path(p_path);
    }

    // Use UsdStageManager to save
    bool success = UsdStageManager::get_singleton().save_stage(_stage_id, abs_path.utf8().get_data());

    if (!success) {
        UtilityFunctions::printerr("UsdStageProxy: Failed to save stage");
        return ERR_CANT_CREATE;
    }

    if (!p_path.is_empty()) {
        _file_path = p_path;
    }

    return OK;
}

Error UsdStageProxy::export_to(const String &p_path, bool p_binary) {
    if (_stage_id == 0) {
        UtilityFunctions::printerr("UsdStageProxy: No stage open");
        return ERR_UNCONFIGURED;
    }

    // Convert Godot path to filesystem path
    String abs_path = p_path;
    if (p_path.begins_with("res://") || p_path.begins_with("user://")) {
        abs_path = ProjectSettings::get_singleton()->globalize_path(p_path);
    }

    // Use UsdStageManager to save
    bool success = UsdStageManager::get_singleton().save_stage(_stage_id, abs_path.utf8().get_data());

    if (!success) {
        UtilityFunctions::printerr("UsdStageProxy: Failed to export stage");
        return ERR_CANT_CREATE;
    }

    return OK;
}

void UsdStageProxy::close() {
    if (_stage_id != 0) {
        UsdStageManager::get_singleton().close_stage(_stage_id);
        _stage_id = 0;
        _file_path = String();
    }
}

Error UsdStageProxy::reload() {
    if (_stage_id == 0) {
        UtilityFunctions::printerr("UsdStageProxy: No stage open");
        return ERR_UNCONFIGURED;
    }

    // Close and reopen
    String path = _file_path;
    close();
    return open(path);
}

bool UsdStageProxy::is_open() const {
    return _stage_id != 0;
}

bool UsdStageProxy::is_modified() const {
    return get_generation() > 0;
}

// -----------------------------------------------------------------------------
// Prim Access
// -----------------------------------------------------------------------------

Ref<UsdPrimProxy> UsdStageProxy::get_default_prim() const {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return Ref<UsdPrimProxy>();
    }

    pxr::UsdPrim default_prim = record->get_stage()->GetDefaultPrim();
    if (!default_prim.IsValid()) {
        return Ref<UsdPrimProxy>();
    }

    return UsdPrimProxy::create(default_prim, record->get_stage());
}

Error UsdStageProxy::set_default_prim(const String &p_prim_path) {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return ERR_UNCONFIGURED;
    }

    pxr::SdfPath path(p_prim_path.utf8().get_data());
    pxr::UsdPrim prim = record->get_stage()->GetPrimAtPath(path);
    if (!prim.IsValid()) {
        UtilityFunctions::printerr("UsdStageProxy: Prim not found at path: ", p_prim_path);
        return ERR_DOES_NOT_EXIST;
    }

    record->get_stage()->SetDefaultPrim(prim);
    record->mark_modified();
    return OK;
}

Ref<UsdPrimProxy> UsdStageProxy::get_prim_at_path(const String &p_path) const {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return Ref<UsdPrimProxy>();
    }

    pxr::SdfPath path(p_path.utf8().get_data());
    pxr::UsdPrim prim = record->get_stage()->GetPrimAtPath(path);
    if (!prim.IsValid()) {
        return Ref<UsdPrimProxy>();
    }

    return UsdPrimProxy::create(prim, record->get_stage());
}

bool UsdStageProxy::has_prim_at_path(const String &p_path) const {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return false;
    }

    pxr::SdfPath path(p_path.utf8().get_data());
    pxr::UsdPrim prim = record->get_stage()->GetPrimAtPath(path);
    return prim.IsValid();
}

Array UsdStageProxy::traverse() const {
    Array result;
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return result;
    }

    for (const pxr::UsdPrim &prim : record->get_stage()->Traverse()) {
        result.push_back(UsdPrimProxy::create(prim, record->get_stage()));
    }

    return result;
}

Array UsdStageProxy::traverse_by_type(const String &p_type_name) const {
    Array result;
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return result;
    }

    pxr::TfToken type_token(p_type_name.utf8().get_data());
    for (const pxr::UsdPrim &prim : record->get_stage()->Traverse()) {
        if (prim.GetTypeName() == type_token) {
            result.push_back(UsdPrimProxy::create(prim, record->get_stage()));
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
// Prim Creation
// -----------------------------------------------------------------------------

Ref<UsdPrimProxy> UsdStageProxy::define_prim(const String &p_path, const String &p_type_name) {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        UtilityFunctions::printerr("UsdStageProxy: No stage open");
        return Ref<UsdPrimProxy>();
    }

    pxr::SdfPath path(p_path.utf8().get_data());
    pxr::TfToken type_token(p_type_name.utf8().get_data());

    try {
        pxr::UsdPrim prim = record->get_stage()->DefinePrim(path, type_token);
        if (!prim.IsValid()) {
            UtilityFunctions::printerr("UsdStageProxy: Failed to define prim at ", p_path);
            return Ref<UsdPrimProxy>();
        }
        record->mark_modified();
        return UsdPrimProxy::create(prim, record->get_stage());
    } catch (const std::exception &e) {
        UtilityFunctions::printerr("UsdStageProxy: Exception defining prim: ", e.what());
        return Ref<UsdPrimProxy>();
    }
}

Error UsdStageProxy::remove_prim(const String &p_path) {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return ERR_UNCONFIGURED;
    }

    pxr::SdfPath path(p_path.utf8().get_data());

    try {
        if (record->get_stage()->RemovePrim(path)) {
            record->mark_modified();
            return OK;
        }
        return ERR_CANT_RESOLVE;
    } catch (const std::exception &e) {
        UtilityFunctions::printerr("UsdStageProxy: Exception removing prim: ", e.what());
        return ERR_CANT_RESOLVE;
    }
}

// -----------------------------------------------------------------------------
// Time / Animation
// -----------------------------------------------------------------------------

void UsdStageProxy::set_time_code(double p_time) {
    // Store current time code for evaluation (not a USD concept per-se,
    // but useful for our proxy to track "current" time)
    _current_time_code = p_time;
}

double UsdStageProxy::get_time_code() const {
    return _current_time_code;
}

double UsdStageProxy::get_start_time_code() const {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return 0.0;
    }
    return record->get_stage()->GetStartTimeCode();
}

double UsdStageProxy::get_end_time_code() const {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return 0.0;
    }
    return record->get_stage()->GetEndTimeCode();
}

void UsdStageProxy::set_time_range(double p_start, double p_end) {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return;
    }
    record->get_stage()->SetStartTimeCode(p_start);
    record->get_stage()->SetEndTimeCode(p_end);
    record->mark_modified();
}

double UsdStageProxy::get_frames_per_second() const {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return 24.0;
    }
    return record->get_stage()->GetFramesPerSecond();
}

void UsdStageProxy::set_frames_per_second(double p_fps) {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return;
    }
    record->get_stage()->SetFramesPerSecond(p_fps);
    record->mark_modified();
}

// -----------------------------------------------------------------------------
// Stage Metadata
// -----------------------------------------------------------------------------

String UsdStageProxy::get_root_layer_path() const {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return String();
    }
    return String(record->get_stage()->GetRootLayer()->GetRealPath().c_str());
}

String UsdStageProxy::get_up_axis() const {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return "Y";
    }
    pxr::TfToken axis = pxr::UsdGeomGetStageUpAxis(record->get_stage());
    return String(axis.GetText());
}

void UsdStageProxy::set_up_axis(const String &p_axis) {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return;
    }
    pxr::TfToken axis_token;
    if (p_axis == "Y" || p_axis == "y") {
        axis_token = pxr::UsdGeomTokens->y;
    } else if (p_axis == "Z" || p_axis == "z") {
        axis_token = pxr::UsdGeomTokens->z;
    } else {
        UtilityFunctions::printerr("UsdStageProxy: Invalid up axis: ", p_axis, " (must be Y or Z)");
        return;
    }
    pxr::UsdGeomSetStageUpAxis(record->get_stage(), axis_token);
    record->mark_modified();
}

double UsdStageProxy::get_meters_per_unit() const {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return 1.0;
    }
    return pxr::UsdGeomGetStageMetersPerUnit(record->get_stage());
}

void UsdStageProxy::set_meters_per_unit(double p_meters_per_unit) {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return;
    }
    pxr::UsdGeomSetStageMetersPerUnit(record->get_stage(), p_meters_per_unit);
    record->mark_modified();
}

// -----------------------------------------------------------------------------
// Layer Management
// -----------------------------------------------------------------------------

PackedStringArray UsdStageProxy::get_sublayer_paths() const {
    PackedStringArray result;
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return result;
    }

    auto root_layer = record->get_stage()->GetRootLayer();
    for (const std::string &path : root_layer->GetSubLayerPaths()) {
        result.push_back(String(path.c_str()));
    }
    return result;
}

Error UsdStageProxy::add_sublayer(const String &p_path) {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return ERR_UNCONFIGURED;
    }

    try {
        auto root_layer = record->get_stage()->GetRootLayer();
        root_layer->InsertSubLayerPath(p_path.utf8().get_data());
        record->mark_modified();
        return OK;
    } catch (const std::exception &e) {
        UtilityFunctions::printerr("UsdStageProxy: Exception adding sublayer: ", e.what());
        return ERR_CANT_CREATE;
    }
}

Error UsdStageProxy::remove_sublayer(const String &p_path) {
    StageRecord* record = get_stage_record();
    if (!record || !record->get_stage()) {
        return ERR_UNCONFIGURED;
    }

    try {
        auto root_layer = record->get_stage()->GetRootLayer();
        std::string path_str = p_path.utf8().get_data();

        // Find and remove the sublayer
        auto sublayers = root_layer->GetSubLayerPaths();
        for (size_t i = 0; i < sublayers.size(); ++i) {
            if (sublayers[i] == path_str) {
                root_layer->RemoveSubLayerPath(i);
                record->mark_modified();
                return OK;
            }
        }
        return ERR_DOES_NOT_EXIST;
    } catch (const std::exception &e) {
        UtilityFunctions::printerr("UsdStageProxy: Exception removing sublayer: ", e.what());
        return ERR_CANT_RESOLVE;
    }
}

// -----------------------------------------------------------------------------
// Prim Attributes and Transforms (Shared with MCP)
// -----------------------------------------------------------------------------

Error UsdStageProxy::set_prim_attribute(const String &p_prim_path, const String &p_attr_name,
                                        const String &p_value_type, const String &p_value) {
    if (_stage_id == 0) {
        UtilityFunctions::printerr("UsdStageProxy: No stage open");
        return ERR_UNCONFIGURED;
    }

    bool success = UsdStageManager::get_singleton().set_prim_attribute(
        _stage_id, p_prim_path.utf8().get_data(), p_attr_name.utf8().get_data(),
        p_value_type.utf8().get_data(), p_value.utf8().get_data());

    return success ? OK : ERR_CANT_CREATE;
}

Dictionary UsdStageProxy::get_prim_attribute(const String &p_prim_path, const String &p_attr_name) const {
    Dictionary result;

    if (_stage_id == 0) {
        UtilityFunctions::printerr("UsdStageProxy: No stage open");
        return result;
    }

    std::string value, value_type;
    bool success = UsdStageManager::get_singleton().get_prim_attribute(
        _stage_id, p_prim_path.utf8().get_data(), p_attr_name.utf8().get_data(),
        value, value_type);

    if (success) {
        result["value"] = String(value.c_str());
        result["type"] = String(value_type.c_str());
    }

    return result;
}

Error UsdStageProxy::set_prim_transform(const String &p_prim_path,
                                        double p_tx, double p_ty, double p_tz,
                                        double p_rx, double p_ry, double p_rz,
                                        double p_sx, double p_sy, double p_sz) {
    if (_stage_id == 0) {
        UtilityFunctions::printerr("UsdStageProxy: No stage open");
        return ERR_UNCONFIGURED;
    }

    bool success = UsdStageManager::get_singleton().set_prim_transform(
        _stage_id, p_prim_path.utf8().get_data(),
        p_tx, p_ty, p_tz, p_rx, p_ry, p_rz, p_sx, p_sy, p_sz);

    return success ? OK : ERR_CANT_CREATE;
}

PackedStringArray UsdStageProxy::list_prims() const {
    PackedStringArray result;

    if (_stage_id == 0) {
        return result;
    }

    std::vector<std::string> prims = UsdStageManager::get_singleton().list_prims(_stage_id);

    for (const std::string& prim_path : prims) {
        result.push_back(String(prim_path.c_str()));
    }

    return result;
}

// -----------------------------------------------------------------------------
// Shared State (MCP Interop)
// -----------------------------------------------------------------------------

int64_t UsdStageProxy::get_stage_id() const {
    return _stage_id;
}

int64_t UsdStageProxy::get_generation() const {
    if (_stage_id == 0) {
        return 0;
    }
    return UsdStageManager::get_singleton().get_generation(_stage_id);
}

StageRecord* UsdStageProxy::get_stage_record() const {
    if (_stage_id == 0) {
        return nullptr;
    }
    return UsdStageManager::get_singleton().get_stage_record(_stage_id);
}

// -----------------------------------------------------------------------------
// Convenience Methods
// -----------------------------------------------------------------------------

Node *UsdStageProxy::import_to_scene(Node *p_parent) const {
    // TODO: Implement using UsdDocument's import logic
    UtilityFunctions::printerr("UsdStageProxy: import_to_scene not yet implemented");
    return nullptr;
}

Error UsdStageProxy::export_from_scene(Node *p_root) {
    // TODO: Implement using UsdDocument's export logic
    UtilityFunctions::printerr("UsdStageProxy: export_from_scene not yet implemented");
    return ERR_UNAVAILABLE;
}

} // namespace godot
