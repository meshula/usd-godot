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

UsdStageProxy::UsdStageProxy() : _is_modified(false), _current_time_code(0.0) {
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

    try {
        _stage = pxr::UsdStage::Open(abs_path.utf8().get_data());
        if (!_stage) {
            UtilityFunctions::printerr("UsdStageProxy: Failed to open stage at ", p_path);
            return ERR_CANT_OPEN;
        }
        _file_path = p_path;
        _is_modified = false;
        return OK;
    } catch (const std::exception &e) {
        UtilityFunctions::printerr("UsdStageProxy: Exception opening stage: ", e.what());
        return ERR_CANT_OPEN;
    }
}

Error UsdStageProxy::create_new(const String &p_path) {
    // Close any existing stage
    close();

    // Convert Godot path to filesystem path
    String abs_path = p_path;
    if (p_path.begins_with("res://") || p_path.begins_with("user://")) {
        abs_path = ProjectSettings::get_singleton()->globalize_path(p_path);
    }

    try {
        _stage = pxr::UsdStage::CreateNew(abs_path.utf8().get_data());
        if (!_stage) {
            UtilityFunctions::printerr("UsdStageProxy: Failed to create stage at ", p_path);
            return ERR_CANT_CREATE;
        }
        _file_path = p_path;
        _is_modified = true;

        // Set up default stage metadata
        pxr::UsdGeomSetStageUpAxis(_stage, pxr::UsdGeomTokens->y);
        pxr::UsdGeomSetStageMetersPerUnit(_stage, 1.0);

        return OK;
    } catch (const std::exception &e) {
        UtilityFunctions::printerr("UsdStageProxy: Exception creating stage: ", e.what());
        return ERR_CANT_CREATE;
    }
}

Error UsdStageProxy::save(const String &p_path) {
    if (!_stage) {
        UtilityFunctions::printerr("UsdStageProxy: No stage open");
        return ERR_UNCONFIGURED;
    }

    try {
        if (p_path.is_empty()) {
            // Save to original location
            _stage->Save();
        } else {
            // Convert Godot path to filesystem path
            String abs_path = p_path;
            if (p_path.begins_with("res://") || p_path.begins_with("user://")) {
                abs_path = ProjectSettings::get_singleton()->globalize_path(p_path);
            }
            _stage->GetRootLayer()->Export(abs_path.utf8().get_data());
            _file_path = p_path;
        }
        _is_modified = false;
        return OK;
    } catch (const std::exception &e) {
        UtilityFunctions::printerr("UsdStageProxy: Exception saving stage: ", e.what());
        return ERR_CANT_CREATE;
    }
}

Error UsdStageProxy::export_to(const String &p_path, bool p_binary) {
    if (!_stage) {
        UtilityFunctions::printerr("UsdStageProxy: No stage open");
        return ERR_UNCONFIGURED;
    }

    // Convert Godot path to filesystem path
    String abs_path = p_path;
    if (p_path.begins_with("res://") || p_path.begins_with("user://")) {
        abs_path = ProjectSettings::get_singleton()->globalize_path(p_path);
    }

    try {
        _stage->Export(abs_path.utf8().get_data());
        return OK;
    } catch (const std::exception &e) {
        UtilityFunctions::printerr("UsdStageProxy: Exception exporting stage: ", e.what());
        return ERR_CANT_CREATE;
    }
}

void UsdStageProxy::close() {
    _stage = nullptr;
    _file_path = String();
    _is_modified = false;
}

Error UsdStageProxy::reload() {
    if (!_stage) {
        UtilityFunctions::printerr("UsdStageProxy: No stage open");
        return ERR_UNCONFIGURED;
    }

    try {
        _stage->Reload();
        _is_modified = false;
        return OK;
    } catch (const std::exception &e) {
        UtilityFunctions::printerr("UsdStageProxy: Exception reloading stage: ", e.what());
        return ERR_CANT_OPEN;
    }
}

bool UsdStageProxy::is_open() const {
    return _stage != nullptr;
}

bool UsdStageProxy::is_modified() const {
    return _is_modified;
}

// -----------------------------------------------------------------------------
// Prim Access
// -----------------------------------------------------------------------------

Ref<UsdPrimProxy> UsdStageProxy::get_default_prim() const {
    if (!_stage) {
        return Ref<UsdPrimProxy>();
    }

    pxr::UsdPrim default_prim = _stage->GetDefaultPrim();
    if (!default_prim.IsValid()) {
        return Ref<UsdPrimProxy>();
    }

    return UsdPrimProxy::create(default_prim, _stage);
}

Error UsdStageProxy::set_default_prim(const String &p_prim_path) {
    if (!_stage) {
        return ERR_UNCONFIGURED;
    }

    pxr::SdfPath path(p_prim_path.utf8().get_data());
    pxr::UsdPrim prim = _stage->GetPrimAtPath(path);
    if (!prim.IsValid()) {
        UtilityFunctions::printerr("UsdStageProxy: Prim not found at path: ", p_prim_path);
        return ERR_DOES_NOT_EXIST;
    }

    _stage->SetDefaultPrim(prim);
    _is_modified = true;
    return OK;
}

Ref<UsdPrimProxy> UsdStageProxy::get_prim_at_path(const String &p_path) const {
    if (!_stage) {
        return Ref<UsdPrimProxy>();
    }

    pxr::SdfPath path(p_path.utf8().get_data());
    pxr::UsdPrim prim = _stage->GetPrimAtPath(path);
    if (!prim.IsValid()) {
        return Ref<UsdPrimProxy>();
    }

    return UsdPrimProxy::create(prim, _stage);
}

bool UsdStageProxy::has_prim_at_path(const String &p_path) const {
    if (!_stage) {
        return false;
    }

    pxr::SdfPath path(p_path.utf8().get_data());
    pxr::UsdPrim prim = _stage->GetPrimAtPath(path);
    return prim.IsValid();
}

Array UsdStageProxy::traverse() const {
    Array result;
    if (!_stage) {
        return result;
    }

    for (const pxr::UsdPrim &prim : _stage->Traverse()) {
        result.push_back(UsdPrimProxy::create(prim, _stage));
    }

    return result;
}

Array UsdStageProxy::traverse_by_type(const String &p_type_name) const {
    Array result;
    if (!_stage) {
        return result;
    }

    pxr::TfToken type_token(p_type_name.utf8().get_data());
    for (const pxr::UsdPrim &prim : _stage->Traverse()) {
        if (prim.GetTypeName() == type_token) {
            result.push_back(UsdPrimProxy::create(prim, _stage));
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
// Prim Creation
// -----------------------------------------------------------------------------

Ref<UsdPrimProxy> UsdStageProxy::define_prim(const String &p_path, const String &p_type_name) {
    if (!_stage) {
        UtilityFunctions::printerr("UsdStageProxy: No stage open");
        return Ref<UsdPrimProxy>();
    }

    pxr::SdfPath path(p_path.utf8().get_data());
    pxr::TfToken type_token(p_type_name.utf8().get_data());

    try {
        pxr::UsdPrim prim = _stage->DefinePrim(path, type_token);
        if (!prim.IsValid()) {
            UtilityFunctions::printerr("UsdStageProxy: Failed to define prim at ", p_path);
            return Ref<UsdPrimProxy>();
        }
        _is_modified = true;
        return UsdPrimProxy::create(prim, _stage);
    } catch (const std::exception &e) {
        UtilityFunctions::printerr("UsdStageProxy: Exception defining prim: ", e.what());
        return Ref<UsdPrimProxy>();
    }
}

Error UsdStageProxy::remove_prim(const String &p_path) {
    if (!_stage) {
        return ERR_UNCONFIGURED;
    }

    pxr::SdfPath path(p_path.utf8().get_data());

    try {
        if (_stage->RemovePrim(path)) {
            _is_modified = true;
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
    if (!_stage) {
        return 0.0;
    }
    return _stage->GetStartTimeCode();
}

double UsdStageProxy::get_end_time_code() const {
    if (!_stage) {
        return 0.0;
    }
    return _stage->GetEndTimeCode();
}

void UsdStageProxy::set_time_range(double p_start, double p_end) {
    if (!_stage) {
        return;
    }
    _stage->SetStartTimeCode(p_start);
    _stage->SetEndTimeCode(p_end);
    _is_modified = true;
}

double UsdStageProxy::get_frames_per_second() const {
    if (!_stage) {
        return 24.0;
    }
    return _stage->GetFramesPerSecond();
}

void UsdStageProxy::set_frames_per_second(double p_fps) {
    if (!_stage) {
        return;
    }
    _stage->SetFramesPerSecond(p_fps);
    _is_modified = true;
}

// -----------------------------------------------------------------------------
// Stage Metadata
// -----------------------------------------------------------------------------

String UsdStageProxy::get_root_layer_path() const {
    if (!_stage) {
        return String();
    }
    return String(_stage->GetRootLayer()->GetRealPath().c_str());
}

String UsdStageProxy::get_up_axis() const {
    if (!_stage) {
        return "Y";
    }
    pxr::TfToken axis = pxr::UsdGeomGetStageUpAxis(_stage);
    return String(axis.GetText());
}

void UsdStageProxy::set_up_axis(const String &p_axis) {
    if (!_stage) {
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
    pxr::UsdGeomSetStageUpAxis(_stage, axis_token);
    _is_modified = true;
}

double UsdStageProxy::get_meters_per_unit() const {
    if (!_stage) {
        return 1.0;
    }
    return pxr::UsdGeomGetStageMetersPerUnit(_stage);
}

void UsdStageProxy::set_meters_per_unit(double p_meters_per_unit) {
    if (!_stage) {
        return;
    }
    pxr::UsdGeomSetStageMetersPerUnit(_stage, p_meters_per_unit);
    _is_modified = true;
}

// -----------------------------------------------------------------------------
// Layer Management
// -----------------------------------------------------------------------------

PackedStringArray UsdStageProxy::get_sublayer_paths() const {
    PackedStringArray result;
    if (!_stage) {
        return result;
    }

    auto root_layer = _stage->GetRootLayer();
    for (const std::string &path : root_layer->GetSubLayerPaths()) {
        result.push_back(String(path.c_str()));
    }
    return result;
}

Error UsdStageProxy::add_sublayer(const String &p_path) {
    if (!_stage) {
        return ERR_UNCONFIGURED;
    }

    try {
        auto root_layer = _stage->GetRootLayer();
        root_layer->InsertSubLayerPath(p_path.utf8().get_data());
        _is_modified = true;
        return OK;
    } catch (const std::exception &e) {
        UtilityFunctions::printerr("UsdStageProxy: Exception adding sublayer: ", e.what());
        return ERR_CANT_CREATE;
    }
}

Error UsdStageProxy::remove_sublayer(const String &p_path) {
    if (!_stage) {
        return ERR_UNCONFIGURED;
    }

    try {
        auto root_layer = _stage->GetRootLayer();
        std::string path_str = p_path.utf8().get_data();

        // Find and remove the sublayer
        auto sublayers = root_layer->GetSubLayerPaths();
        for (size_t i = 0; i < sublayers.size(); ++i) {
            if (sublayers[i] == path_str) {
                root_layer->RemoveSubLayerPath(i);
                _is_modified = true;
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
