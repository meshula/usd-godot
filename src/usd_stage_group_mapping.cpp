#include "usd_stage_group_mapping.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

UsdStageGroupMapping* UsdStageGroupMapping::singleton_ = nullptr;

UsdStageGroupMapping* UsdStageGroupMapping::get_singleton() {
    if (!singleton_) {
        singleton_ = new UsdStageGroupMapping();
    }
    return singleton_;
}

UsdStageGroupMapping::UsdStageGroupMapping() {
    // Store in project root (res://) for team collaboration
    // The file can be committed to git for shared mappings
    String project_path = ProjectSettings::get_singleton()->globalize_path("res://");
    mappings_file_path_ = project_path + "/.usd_stage_mappings.json";

    UtilityFunctions::print("USD Stage Mappings: Initializing (file: ", mappings_file_path_, ")");
    load_from_file();
    UtilityFunctions::print("USD Stage Mappings: Ready with ", static_cast<int64_t>(mappings_.size()), " mappings");
}

UsdStageGroupMapping::~UsdStageGroupMapping() {
    save_to_file();
}

void UsdStageGroupMapping::set_mapping(const String& file_path, const String& group_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = file_path.utf8().get_data();

    if (mappings_.find(key) != mappings_.end()) {
        // Update existing mapping
        mappings_[key].group_name = group_name;
        // Keep existing generation
    } else {
        // Create new mapping
        mappings_[key] = GroupInfo(group_name, 0);
    }

    save_to_file();
}

String UsdStageGroupMapping::get_group_name(const String& file_path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = file_path.utf8().get_data();
    auto it = mappings_.find(key);
    if (it != mappings_.end()) {
        return it->second.group_name;
    }
    return String();
}

bool UsdStageGroupMapping::has_mapping(const String& file_path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = file_path.utf8().get_data();
    return mappings_.find(key) != mappings_.end();
}

bool UsdStageGroupMapping::needs_update(const String& file_path, uint64_t current_generation) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = file_path.utf8().get_data();
    auto it = mappings_.find(key);
    if (it != mappings_.end()) {
        return current_generation > it->second.last_generation;
    }
    return false;  // No mapping = no update needed
}

void UsdStageGroupMapping::update_generation(const String& file_path, uint64_t new_generation) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = file_path.utf8().get_data();
    auto it = mappings_.find(key);
    if (it != mappings_.end()) {
        it->second.last_generation = new_generation;
        save_to_file();
    }
}

void UsdStageGroupMapping::remove_mapping(const String& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = file_path.utf8().get_data();
    mappings_.erase(key);
    save_to_file();
}

Array UsdStageGroupMapping::get_all_mappings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Array result;

    for (const auto& pair : mappings_) {
        Dictionary mapping;
        mapping["file_path"] = String(pair.first.c_str());
        mapping["group_name"] = pair.second.group_name;
        mapping["last_generation"] = static_cast<int64_t>(pair.second.last_generation);
        result.push_back(mapping);
    }

    return result;
}

bool UsdStageGroupMapping::save_to_file() {
    // Build JSON structure
    Dictionary root;
    Array mappings_array;

    for (const auto& pair : mappings_) {
        Dictionary mapping;
        mapping["file_path"] = String(pair.first.c_str());
        mapping["group_name"] = pair.second.group_name;
        mapping["last_generation"] = static_cast<int64_t>(pair.second.last_generation);
        mappings_array.push_back(mapping);
    }

    root["mappings"] = mappings_array;
    root["version"] = 1;

    // Convert to JSON string
    Ref<JSON> json = memnew(JSON);
    String json_string = json->stringify(root, "  ");

    // Write to file
    Ref<FileAccess> file = FileAccess::open(mappings_file_path_, FileAccess::WRITE);
    if (file.is_valid()) {
        file->store_string(json_string);
        file->close();
        UtilityFunctions::print("USD Stage Mappings: Saved ", static_cast<int64_t>(mappings_.size()), " mappings to ", mappings_file_path_);
        return true;
    } else {
        UtilityFunctions::printerr("USD Stage Mappings: Failed to save to ", mappings_file_path_);
        return false;
    }
}

bool UsdStageGroupMapping::load_from_file() {
    Ref<FileAccess> file = FileAccess::open(mappings_file_path_, FileAccess::READ);
    if (!file.is_valid()) {
        UtilityFunctions::print("USD Stage Mappings: No existing mappings file (first run)");
        return false;
    }

    String json_string = file->get_as_text();
    file->close();

    if (json_string.is_empty()) {
        UtilityFunctions::print("USD Stage Mappings: Empty mappings file");
        return false;
    }

    // Parse JSON
    Ref<JSON> json = memnew(JSON);
    Error parse_error = json->parse(json_string);
    if (parse_error != OK) {
        UtilityFunctions::printerr("USD Stage Mappings: Failed to parse JSON at line ", json->get_error_line(), ": ", json->get_error_message());
        return false;
    }

    Dictionary root = json->get_data();
    if (!root.has("mappings")) {
        UtilityFunctions::printerr("USD Stage Mappings: Invalid format - missing 'mappings' key");
        return false;
    }

    Array mappings_array = root["mappings"];
    mappings_.clear();

    for (int i = 0; i < mappings_array.size(); i++) {
        Dictionary mapping = mappings_array[i];
        if (mapping.has("file_path") && mapping.has("group_name")) {
            String file_path = mapping["file_path"];
            String group_name = mapping["group_name"];
            uint64_t last_generation = mapping.has("last_generation") ?
                static_cast<uint64_t>(static_cast<int64_t>(mapping["last_generation"])) : 0;

            std::string key = file_path.utf8().get_data();
            mappings_[key] = GroupInfo(group_name, last_generation);
        }
    }

    UtilityFunctions::print("USD Stage Mappings: Loaded ", static_cast<int64_t>(mappings_.size()), " mappings from ", mappings_file_path_);
    return true;
}

} // namespace godot
