#ifndef USD_GODOT_STAGE_MANAGER_H
#define USD_GODOT_STAGE_MANAGER_H

#include <pxr/usd/usd/stage.h>
#include <string>
#include <map>
#include <mutex>
#include <cstdint>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd_godot {

// Unique identifier for stages
using StageId = uint64_t;

// Stage record with generation tracking
// Generation is incremented on any mutation to help track if stage needs saving
class StageRecord {
public:
    StageRecord(UsdStageRefPtr stage)
        : stage_(stage), generation_(0) {}

    // Read-only access
    UsdStageRefPtr get_stage() const { return stage_; }
    uint64_t get_generation() const { return generation_; }

    // Mutating operations - these increment generation
    void mark_modified() { generation_++; }

    // Helper to create a prim (increments generation)
    UsdPrim create_prim(const std::string& path, const std::string& type_name);

    // Helper to define a prim (increments generation)
    UsdPrim define_prim(const std::string& path, const std::string& type_name);

    // Get prim (read-only, does NOT increment generation)
    UsdPrim get_prim(const std::string& path) const;

    // Set prim attribute (increments generation)
    bool set_attribute(const std::string& prim_path, const std::string& attr_name,
                      const std::string& value_type, const std::string& value);

    // Get prim attribute (read-only, does NOT increment generation)
    bool get_attribute(const std::string& prim_path, const std::string& attr_name,
                      std::string& out_value, std::string& out_type) const;

    // Set transform (increments generation)
    bool set_transform(const std::string& prim_path,
                      double tx, double ty, double tz,
                      double rx, double ry, double rz,
                      double sx, double sy, double sz);

    // Save stage (does NOT increment generation - saving doesn't change the stage)
    bool save();
    bool export_to_string(std::string& out_string);

private:
    UsdStageRefPtr stage_;
    uint64_t generation_;
};

// Central stage manager - shared between MCP server and GDScript bindings
// Thread-safe singleton
class UsdStageManager {
public:
    static UsdStageManager& get_singleton();

    // Create a new stage
    StageId create_stage(const std::string& file_path = "");

    // Open an existing stage
    StageId open_stage(const std::string& file_path);

    // Get stage record (returns nullptr if not found)
    StageRecord* get_stage_record(StageId id);

    // Close a stage
    bool close_stage(StageId id);

    // Save stage to file
    bool save_stage(StageId id, const std::string& file_path = "");

    // Get generation number
    uint64_t get_generation(StageId id);

    // Create prim in stage (convenience method)
    bool create_prim(StageId id, const std::string& path, const std::string& type_name);

    // Set prim attribute in stage (convenience method)
    bool set_prim_attribute(StageId id, const std::string& prim_path,
                           const std::string& attr_name, const std::string& value_type,
                           const std::string& value);

    // Get prim attribute from stage (convenience method)
    bool get_prim_attribute(StageId id, const std::string& prim_path,
                           const std::string& attr_name, std::string& out_value,
                           std::string& out_type);

    // Set prim transform (convenience method)
    bool set_prim_transform(StageId id, const std::string& prim_path,
                           double tx, double ty, double tz,
                           double rx, double ry, double rz,
                           double sx, double sy, double sz);

    // List all prims in stage (convenience method)
    std::vector<std::string> list_prims(StageId id);

    // Get all active stage IDs
    std::vector<StageId> get_active_stages() const;

private:
    UsdStageManager() : next_id_(1) {}
    ~UsdStageManager() = default;

    // Prevent copying
    UsdStageManager(const UsdStageManager&) = delete;
    UsdStageManager& operator=(const UsdStageManager&) = delete;

    std::map<StageId, StageRecord> stages_;
    StageId next_id_;
    mutable std::mutex mutex_;
};

} // namespace usd_godot

#endif // USD_GODOT_STAGE_MANAGER_H
