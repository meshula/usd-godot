#include "usd_stage_manager.h"

#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usd/primRange.h>

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

} // namespace usd_godot
