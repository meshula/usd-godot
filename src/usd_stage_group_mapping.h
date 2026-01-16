#ifndef USD_STAGE_GROUP_MAPPING_H
#define USD_STAGE_GROUP_MAPPING_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <map>
#include <mutex>
#include <cstdint>
#include <string>

namespace godot {

// Singleton class for managing USD file to Godot scene group mappings
// Maps USD file paths to scene groups for team collaboration
class UsdStageGroupMapping {
public:
    // Information about a mapped group
    struct GroupInfo {
        String group_name;           // Godot scene group name
        uint64_t last_generation;    // Generation at last reflection

        GroupInfo() : last_generation(0) {}
        GroupInfo(const String& name, uint64_t gen)
            : group_name(name), last_generation(gen) {}
    };

    // Singleton access
    static UsdStageGroupMapping* get_singleton();

    // Delete copy/move constructors
    UsdStageGroupMapping(const UsdStageGroupMapping&) = delete;
    UsdStageGroupMapping& operator=(const UsdStageGroupMapping&) = delete;

    // Mapping management (keyed by USD file path)
    void set_mapping(const String& file_path, const String& group_name);
    String get_group_name(const String& file_path) const;
    bool has_mapping(const String& file_path) const;
    bool needs_update(const String& file_path, uint64_t current_generation) const;
    void update_generation(const String& file_path, uint64_t new_generation);
    void remove_mapping(const String& file_path);
    Array get_all_mappings() const;  // For MCP - returns array of dictionaries

    // Persistence - stores in project root .usd_stage_mappings.json
    bool save_to_file();
    bool load_from_file();

private:
    UsdStageGroupMapping();
    ~UsdStageGroupMapping();

    static UsdStageGroupMapping* singleton_;
    std::map<std::string, GroupInfo> mappings_;  // Key: USD file path (std::string for map key)
    mutable std::mutex mutex_;  // Thread-safe access
    String mappings_file_path_;
};

} // namespace godot

#endif // USD_STAGE_GROUP_MAPPING_H
