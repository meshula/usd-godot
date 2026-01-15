#include "usd_stage_manager.h"

#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformCommonAPI.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/rotation.h>

#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace usd_godot {

// ============================================================================
// StageRecord Implementation
// ============================================================================

UsdPrim StageRecord::create_prim(const std::string& path, const std::string& type_name) {
    if (!stage_) {
        return UsdPrim();
    }

    SdfPath sdf_path(path);
    TfToken type_token(type_name);

    // Create the prim with the specified type
    UsdPrim prim = stage_->DefinePrim(sdf_path, type_token);

    if (prim) {
        generation_++;  // Increment generation on successful creation
    }

    return prim;
}

UsdPrim StageRecord::define_prim(const std::string& path, const std::string& type_name) {
    if (!stage_) {
        return UsdPrim();
    }

    SdfPath sdf_path(path);

    UsdPrim prim;
    if (type_name.empty()) {
        prim = stage_->DefinePrim(sdf_path);
    } else {
        TfToken type_token(type_name);
        prim = stage_->DefinePrim(sdf_path, type_token);
    }

    if (prim) {
        generation_++;  // Increment generation on successful definition
    }

    return prim;
}

bool StageRecord::save() {
    if (!stage_) {
        return false;
    }

    // Save does NOT increment generation - saving doesn't modify the stage content
    stage_->Save();
    return true;
}

bool StageRecord::export_to_string(std::string& out_string) {
    if (!stage_) {
        return false;
    }

    // Export to string does NOT increment generation
    stage_->ExportToString(&out_string);
    return true;
}

UsdPrim StageRecord::get_prim(const std::string& path) const {
    if (!stage_) {
        return UsdPrim();
    }

    // Get prim does NOT increment generation - read-only operation
    SdfPath sdf_path(path);
    return stage_->GetPrimAtPath(sdf_path);
}

bool StageRecord::set_attribute(const std::string& prim_path, const std::string& attr_name,
                                const std::string& value_type, const std::string& value) {
    if (!stage_) {
        return false;
    }

    UsdPrim prim = get_prim(prim_path);
    if (!prim) {
        return false;
    }

    // Create or get the attribute
    TfToken attr_token(attr_name);
    UsdAttribute attr = prim.GetAttribute(attr_token);

    if (!attr) {
        // Need to create the attribute with the specified type
        SdfValueTypeName type_name = SdfSchema::GetInstance().FindType(value_type);
        if (type_name) {
            attr = prim.CreateAttribute(attr_token, type_name);
        }
    }

    if (!attr) {
        return false;
    }

    // Parse and set the value based on type
    // For simplicity, we'll support string, float, double, int, bool, and vec3
    bool success = false;

    if (value_type == "string") {
        success = attr.Set(value);
    } else if (value_type == "float") {
        success = attr.Set(std::stof(value));
    } else if (value_type == "double") {
        success = attr.Set(std::stod(value));
    } else if (value_type == "int") {
        success = attr.Set(std::stoi(value));
    } else if (value_type == "bool") {
        success = attr.Set(value == "true" || value == "1");
    } else {
        // For complex types, set as string for now
        success = attr.Set(value);
    }

    if (success) {
        generation_++;  // Increment generation on successful attribute set
    }

    return success;
}

bool StageRecord::get_attribute(const std::string& prim_path, const std::string& attr_name,
                                std::string& out_value, std::string& out_type) const {
    if (!stage_) {
        return false;
    }

    UsdPrim prim = get_prim(prim_path);
    if (!prim) {
        return false;
    }

    TfToken attr_token(attr_name);
    UsdAttribute attr = prim.GetAttribute(attr_token);

    if (!attr) {
        return false;
    }

    // Get the type name
    out_type = attr.GetTypeName().GetAsToken().GetString();

    // Get the value and convert to string
    VtValue value;
    if (attr.Get(&value)) {
        std::ostringstream oss;
        oss << value;
        out_value = oss.str();
        return true;
    }

    return false;
}

bool StageRecord::set_transform(const std::string& prim_path,
                                double tx, double ty, double tz,
                                double rx, double ry, double rz,
                                double sx, double sy, double sz) {
    if (!stage_) {
        return false;
    }

    UsdPrim prim = get_prim(prim_path);
    if (!prim) {
        return false;
    }

    // Ensure this is an Xformable prim
    UsdGeomXformable xformable(prim);
    if (!xformable) {
        return false;
    }

    // Use XformCommonAPI for setting transform
    UsdGeomXformCommonAPI xform_api(xformable);

    GfVec3d translation(tx, ty, tz);
    GfVec3f rotation(rx, ry, rz);  // In degrees
    GfVec3f scale(sx, sy, sz);

    // Set the transform using XYZ rotation order
    bool success = xform_api.SetTranslate(translation);
    success &= xform_api.SetRotate(rotation, UsdGeomXformCommonAPI::RotationOrderXYZ);
    success &= xform_api.SetScale(scale);

    if (success) {
        generation_++;  // Increment generation on successful transform set
    }

    return success;
}

// ============================================================================
// UsdStageManager Implementation
// ============================================================================

UsdStageManager& UsdStageManager::get_singleton() {
    static UsdStageManager instance;
    return instance;
}

StageId UsdStageManager::create_stage(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    UsdStageRefPtr stage;

    if (file_path.empty()) {
        // Create an in-memory stage
        stage = UsdStage::CreateInMemory();
    } else {
        // Create a stage with a file path
        stage = UsdStage::CreateNew(file_path);
    }

    if (!stage) {
        UtilityFunctions::printerr("UsdStageManager: Failed to create stage");
        return 0;
    }

    StageId id = next_id_++;
    stages_.emplace(id, StageRecord(stage));

    UtilityFunctions::print(String("UsdStageManager: Created stage with ID ") + String::num_int64(id) +
                           (file_path.empty() ? String(" (in-memory)") : String(" at ") + String(file_path.c_str())));

    return id;
}

StageId UsdStageManager::open_stage(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    UsdStageRefPtr stage = UsdStage::Open(file_path);

    if (!stage) {
        UtilityFunctions::printerr(String("UsdStageManager: Failed to open stage: ") + String(file_path.c_str()));
        return 0;
    }

    StageId id = next_id_++;
    stages_.emplace(id, StageRecord(stage));

    UtilityFunctions::print(String("UsdStageManager: Opened stage with ID ") + String::num_int64(id) +
                           String(" from ") + String(file_path.c_str()));

    return id;
}

StageRecord* UsdStageManager::get_stage_record(StageId id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = stages_.find(id);
    if (it == stages_.end()) {
        return nullptr;
    }

    return &it->second;
}

bool UsdStageManager::close_stage(StageId id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = stages_.find(id);
    if (it == stages_.end()) {
        UtilityFunctions::printerr(String("UsdStageManager: Stage ID not found: ") + String::num_int64(id));
        return false;
    }

    stages_.erase(it);
    UtilityFunctions::print(String("UsdStageManager: Closed stage ID ") + String::num_int64(id));
    return true;
}

bool UsdStageManager::save_stage(StageId id, const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = stages_.find(id);
    if (it == stages_.end()) {
        UtilityFunctions::printerr(String("UsdStageManager: Stage ID not found: ") + String::num_int64(id));
        return false;
    }

    StageRecord& record = it->second;
    UsdStageRefPtr stage = record.get_stage();

    if (!stage) {
        UtilityFunctions::printerr("UsdStageManager: Invalid stage pointer");
        return false;
    }

    // If a new file path is provided, save as that path
    if (!file_path.empty()) {
        if (!stage->Export(file_path)) {
            UtilityFunctions::printerr(String("UsdStageManager: Failed to export stage to ") + String(file_path.c_str()));
            return false;
        }
        UtilityFunctions::print(String("UsdStageManager: Exported stage ID ") + String::num_int64(id) +
                               String(" to ") + String(file_path.c_str()));
    } else {
        // Save to the current file path
        if (!record.save()) {
            UtilityFunctions::printerr(String("UsdStageManager: Failed to save stage ID ") + String::num_int64(id));
            return false;
        }
        UtilityFunctions::print(String("UsdStageManager: Saved stage ID ") + String::num_int64(id));
    }

    return true;
}

uint64_t UsdStageManager::get_generation(StageId id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = stages_.find(id);
    if (it == stages_.end()) {
        return 0;
    }

    return it->second.get_generation();
}

bool UsdStageManager::create_prim(StageId id, const std::string& path, const std::string& type_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = stages_.find(id);
    if (it == stages_.end()) {
        UtilityFunctions::printerr(String("UsdStageManager: Stage ID not found: ") + String::num_int64(id));
        return false;
    }

    StageRecord& record = it->second;
    UsdPrim prim = record.create_prim(path, type_name);

    if (!prim) {
        UtilityFunctions::printerr(String("UsdStageManager: Failed to create prim: ") + String(path.c_str()));
        return false;
    }

    UtilityFunctions::print(String("UsdStageManager: Created prim ") + String(path.c_str()) +
                           String(" of type ") + String(type_name.c_str()) +
                           String(" in stage ") + String::num_int64(id));

    return true;
}

std::vector<StageId> UsdStageManager::get_active_stages() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<StageId> ids;
    ids.reserve(stages_.size());

    for (const auto& pair : stages_) {
        ids.push_back(pair.first);
    }

    return ids;
}

bool UsdStageManager::set_prim_attribute(StageId id, const std::string& prim_path,
                                         const std::string& attr_name, const std::string& value_type,
                                         const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = stages_.find(id);
    if (it == stages_.end()) {
        UtilityFunctions::printerr(String("UsdStageManager: Stage ID not found: ") + String::num_int64(id));
        return false;
    }

    StageRecord& record = it->second;
    bool success = record.set_attribute(prim_path, attr_name, value_type, value);

    if (success) {
        UtilityFunctions::print(String("UsdStageManager: Set attribute ") + String(attr_name.c_str()) +
                               String(" on prim ") + String(prim_path.c_str()) +
                               String(" in stage ") + String::num_int64(id));
    }

    return success;
}

bool UsdStageManager::get_prim_attribute(StageId id, const std::string& prim_path,
                                         const std::string& attr_name, std::string& out_value,
                                         std::string& out_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = stages_.find(id);
    if (it == stages_.end()) {
        UtilityFunctions::printerr(String("UsdStageManager: Stage ID not found: ") + String::num_int64(id));
        return false;
    }

    const StageRecord& record = it->second;
    return record.get_attribute(prim_path, attr_name, out_value, out_type);
}

bool UsdStageManager::set_prim_transform(StageId id, const std::string& prim_path,
                                         double tx, double ty, double tz,
                                         double rx, double ry, double rz,
                                         double sx, double sy, double sz) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = stages_.find(id);
    if (it == stages_.end()) {
        UtilityFunctions::printerr(String("UsdStageManager: Stage ID not found: ") + String::num_int64(id));
        return false;
    }

    StageRecord& record = it->second;
    bool success = record.set_transform(prim_path, tx, ty, tz, rx, ry, rz, sx, sy, sz);

    if (success) {
        UtilityFunctions::print(String("UsdStageManager: Set transform on prim ") + String(prim_path.c_str()) +
                               String(" in stage ") + String::num_int64(id));
    }

    return success;
}

std::vector<std::string> UsdStageManager::list_prims(StageId id) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> prim_paths;

    auto it = stages_.find(id);
    if (it == stages_.end()) {
        UtilityFunctions::printerr(String("UsdStageManager: Stage ID not found: ") + String::num_int64(id));
        return prim_paths;
    }

    const StageRecord& record = it->second;
    UsdStageRefPtr stage = record.get_stage();

    if (!stage) {
        return prim_paths;
    }

    // Traverse all prims
    for (const UsdPrim& prim : stage->Traverse()) {
        prim_paths.push_back(prim.GetPath().GetString());
    }

    return prim_paths;
}

} // namespace usd_godot
