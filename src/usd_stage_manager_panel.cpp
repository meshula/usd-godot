#include "usd_stage_manager_panel.h"
#include "usd_stage_manager.h"
#include "usd_stage_group_mapping.h"
#include "usd_plugin.h"
#include <pxr/usd/usd/stage.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/h_box_container.hpp>

namespace godot {

void UsdStageManagerPanel::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_stage_selected"), &UsdStageManagerPanel::_on_stage_selected);
    ClassDB::bind_method(D_METHOD("_on_open_usd_pressed"), &UsdStageManagerPanel::_on_open_usd_pressed);
    ClassDB::bind_method(D_METHOD("_on_usd_file_selected", "file_path"), &UsdStageManagerPanel::_on_usd_file_selected);
    ClassDB::bind_method(D_METHOD("_on_create_group_pressed"), &UsdStageManagerPanel::_on_create_group_pressed);
    ClassDB::bind_method(D_METHOD("_on_update_scene_pressed"), &UsdStageManagerPanel::_on_update_scene_pressed);
    ClassDB::bind_method(D_METHOD("_on_refresh_pressed"), &UsdStageManagerPanel::_on_refresh_pressed);
    ClassDB::bind_method(D_METHOD("_on_update_timer_timeout"), &UsdStageManagerPanel::_on_update_timer_timeout);
    ClassDB::bind_method(D_METHOD("_on_create_group_confirmed"), &UsdStageManagerPanel::_on_create_group_confirmed);
}

UsdStageManagerPanel::UsdStageManagerPanel() {
    set_name("USD Stage Manager");
}

UsdStageManagerPanel::~UsdStageManagerPanel() {
}

void UsdStageManagerPanel::_ready() {
    // Title
    Label *title = memnew(Label);
    title->set_text("USD Stage Manager");
    title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    title->add_theme_font_size_override("font_size", 16);
    add_child(title);

    // Button row
    HBoxContainer *button_row = memnew(HBoxContainer);
    add_child(button_row);

    open_usd_button = memnew(Button);
    open_usd_button->set_text("Open USD File");
    open_usd_button->set_tooltip_text("Open a USD file into the stage manager");
    open_usd_button->connect("pressed", Callable(this, "_on_open_usd_pressed"));
    button_row->add_child(open_usd_button);

    create_group_button = memnew(Button);
    create_group_button->set_text("Create Group");
    create_group_button->set_tooltip_text("Associate selected stage with a scene group");
    create_group_button->connect("pressed", Callable(this, "_on_create_group_pressed"));
    button_row->add_child(create_group_button);

    update_scene_button = memnew(Button);
    update_scene_button->set_text("Update Scene");
    update_scene_button->set_tooltip_text("Reflect selected stage to its scene group");
    update_scene_button->connect("pressed", Callable(this, "_on_update_scene_pressed"));
    button_row->add_child(update_scene_button);

    refresh_button = memnew(Button);
    refresh_button->set_text("Refresh");
    refresh_button->set_tooltip_text("Refresh stage list");
    refresh_button->connect("pressed", Callable(this, "_on_refresh_pressed"));
    button_row->add_child(refresh_button);

    // Stage tree
    stage_tree = memnew(Tree);
    stage_tree->set_custom_minimum_size(Vector2(0, 200));
    stage_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    stage_tree->set_columns(4);
    stage_tree->set_column_title(0, "Stage");
    stage_tree->set_column_title(1, "Status");
    stage_tree->set_column_title(2, "Group");
    stage_tree->set_column_title(3, "File");
    stage_tree->set_column_titles_visible(true);
    stage_tree->set_hide_root(true);
    stage_tree->connect("item_selected", Callable(this, "_on_stage_selected"));
    add_child(stage_tree);

    // Info label
    info_label = memnew(Label);
    info_label->set_text("No stage selected");
    add_child(info_label);

    // Open file dialog
    open_file_dialog = memnew(EditorFileDialog);
    open_file_dialog->connect("file_selected", Callable(this, "_on_usd_file_selected"));
    open_file_dialog->set_title("Open USD File");
    open_file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
    open_file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
    open_file_dialog->clear_filters();
    open_file_dialog->add_filter("*.usd ; USD Files");
    open_file_dialog->add_filter("*.usda ; USD ASCII Files");
    open_file_dialog->add_filter("*.usdc ; USD Crate Files");
    add_child(open_file_dialog);

    // Create group dialog
    create_group_dialog = memnew(AcceptDialog);
    create_group_dialog->set_title("Create Scene Group");
    create_group_dialog->connect("confirmed", Callable(this, "_on_create_group_confirmed"));
    add_child(create_group_dialog);

    VBoxContainer *dialog_content = memnew(VBoxContainer);
    create_group_dialog->add_child(dialog_content);

    Label *dialog_label = memnew(Label);
    dialog_label->set_text("Enter group name:");
    dialog_content->add_child(dialog_label);

    group_name_input = memnew(LineEdit);
    group_name_input->set_placeholder("e.g. red_car_group");
    dialog_content->add_child(group_name_input);

    // Update timer (refresh every 2 seconds)
    update_timer = memnew(Timer);
    update_timer->set_wait_time(2.0);
    update_timer->set_autostart(true);
    update_timer->connect("timeout", Callable(this, "_on_update_timer_timeout"));
    add_child(update_timer);

    // Initial population
    _update_stage_list();
}

String UsdStageManagerPanel::_get_status_icon(bool has_mapping, bool needs_update) {
    if (!has_mapping) {
        return "🜁";  // Calcination - Stage created, not reflected
    } else if (needs_update) {
        return "🜄";  // Conjunction - Modified since reflection
    } else {
        return "🜃";  // Separation - Reflected, up-to-date
    }
}

String UsdStageManagerPanel::_get_status_description(bool has_mapping, bool needs_update) {
    if (!has_mapping) {
        return "Not reflected";
    } else if (needs_update) {
        return "Modified";
    } else {
        return "Up-to-date";
    }
}

void UsdStageManagerPanel::_update_stage_list() {
    stage_tree->clear();
    TreeItem *root = stage_tree->create_item();

    usd_godot::UsdStageManager& manager = usd_godot::UsdStageManager::get_singleton();
    UsdStageGroupMapping *mapping = UsdStageGroupMapping::get_singleton();
    if (!mapping) {
        UtilityFunctions::print("USD Stage Manager Panel: UsdStageGroupMapping not available");
        return;
    }

    std::vector<usd_godot::StageId> active_stages = manager.get_active_stages();

    for (size_t i = 0; i < active_stages.size(); i++) {
        usd_godot::StageId stage_id = active_stages[i];
        usd_godot::StageRecord* record = manager.get_stage_record(stage_id);
        if (!record) continue;

        // Get stage info
        uint64_t generation = record->get_generation();
        bool is_loaded = record->is_loaded();
        std::string file_path_str = record->get_file_path();
        String file_path = String(file_path_str.c_str());

        // Check mapping status
        bool has_mapping = mapping->has_mapping(file_path);
        String group_name = has_mapping ? mapping->get_group_name(file_path) : "(none)";
        bool needs_update = has_mapping && mapping->needs_update(file_path, generation);

        // Create tree item
        TreeItem *item = stage_tree->create_item(root);
        item->set_text(0, String::num_int64(stage_id));

        // Show loaded status
        String status_text = is_loaded ?
            (_get_status_icon(has_mapping, needs_update) + " " + _get_status_description(has_mapping, needs_update)) :
            "⊙ not loaded";

        item->set_text(1, status_text);
        item->set_text(2, group_name);
        item->set_text(3, file_path.is_empty() ? "(unnamed)" : file_path.get_file());

        // Store metadata
        item->set_metadata(0, stage_id);
        item->set_metadata(3, file_path);

        // Color coding
        if (!has_mapping) {
            item->set_custom_color(1, Color(0.7, 0.7, 0.7));  // Gray
        } else if (needs_update) {
            item->set_custom_color(1, Color(1.0, 0.5, 0.0));  // Orange
        } else {
            item->set_custom_color(1, Color(0.0, 1.0, 0.0));  // Green
        }
    }
}

void UsdStageManagerPanel::_on_stage_selected() {
    TreeItem *selected = stage_tree->get_selected();
    if (!selected) {
        info_label->set_text("No stage selected");
        selected_stage_id = 0;
        selected_file_path = "";
        return;
    }

    selected_stage_id = static_cast<uint64_t>(static_cast<int64_t>(selected->get_metadata(0)));
    selected_file_path = selected->get_metadata(3);

    String group_name = selected->get_text(2);
    String status = selected->get_text(1);

    info_label->set_text("Stage " + String::num_int64(selected_stage_id) + " | " +
                         status + " | Group: " + group_name + " | " + selected_file_path);

    UtilityFunctions::print("USD Stage Manager Panel: Selected stage ", selected_stage_id);
}

void UsdStageManagerPanel::_on_create_group_pressed() {
    if (selected_stage_id == 0 || selected_file_path.is_empty()) {
        UtilityFunctions::printerr("USD Stage Manager Panel: No stage selected");
        return;
    }

    // Pre-fill with file name as default group name
    String default_name = selected_file_path.get_file().get_basename() + "_group";
    group_name_input->set_text(default_name);

    create_group_dialog->popup_centered();
}

void UsdStageManagerPanel::_on_create_group_confirmed() {
    String group_name = group_name_input->get_text().strip_edges();
    if (group_name.is_empty()) {
        UtilityFunctions::printerr("USD Stage Manager Panel: Group name cannot be empty");
        return;
    }

    if (selected_file_path.is_empty()) {
        UtilityFunctions::printerr("USD Stage Manager Panel: No file path for selected stage");
        return;
    }

    // Create mapping
    UsdStageGroupMapping *mapping = UsdStageGroupMapping::get_singleton();
    if (mapping) {
        mapping->set_mapping(selected_file_path, group_name);
        UtilityFunctions::print("USD Stage Manager Panel: Created mapping: ", selected_file_path, " -> ", group_name);
        _update_stage_list();
    }
}

void UsdStageManagerPanel::_on_update_scene_pressed() {
    if (selected_stage_id == 0 || selected_file_path.is_empty()) {
        UtilityFunctions::printerr("USD Stage Manager Panel: No stage selected");
        return;
    }

    UsdStageGroupMapping *mapping = UsdStageGroupMapping::get_singleton();
    if (!mapping || !mapping->has_mapping(selected_file_path)) {
        UtilityFunctions::printerr("USD Stage Manager Panel: No group mapping for this stage. Create one first.");
        return;
    }

    if (!plugin) {
        UtilityFunctions::printerr("USD Stage Manager Panel: Plugin reference not set");
        return;
    }

    String group_name = mapping->get_group_name(selected_file_path);
    UtilityFunctions::print("USD Stage Manager Panel: Updating scene with group '", group_name, "' from ", selected_file_path);

    // Call plugin's import method
    plugin->_import_to_group(selected_file_path, group_name, false);

    // Update generation after import
    usd_godot::UsdStageManager& manager = usd_godot::UsdStageManager::get_singleton();
    uint64_t generation = manager.get_generation(selected_stage_id);
    mapping->update_generation(selected_file_path, generation);

    // Refresh to show updated status
    _update_stage_list();
}

void UsdStageManagerPanel::_on_refresh_pressed() {
    UtilityFunctions::print("USD Stage Manager Panel: Manual refresh requested");
    _update_stage_list();
}

void UsdStageManagerPanel::_on_update_timer_timeout() {
    // Auto-refresh every 2 seconds
    _update_stage_list();
}

void UsdStageManagerPanel::_on_open_usd_pressed() {
    UtilityFunctions::print("USD Stage Manager Panel: Opening USD file dialog");
    open_file_dialog->popup_centered_ratio(0.6);
}

void UsdStageManagerPanel::_on_usd_file_selected(const String &p_file_path) {
    UtilityFunctions::print("USD Stage Manager Panel: Opening USD file: ", p_file_path);

    usd_godot::UsdStageManager& manager = usd_godot::UsdStageManager::get_singleton();

    // Open the stage
    usd_godot::StageId stage_id = manager.open_stage(p_file_path.utf8().get_data());
    if (stage_id == 0) {
        UtilityFunctions::printerr("USD Stage Manager Panel: Failed to open USD file: ", p_file_path);
        return;
    }

    UtilityFunctions::print("USD Stage Manager Panel: Successfully opened stage with ID: ", stage_id);

    // Refresh the list to show the new stage
    _update_stage_list();
}

} // namespace godot
