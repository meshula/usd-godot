#ifndef USD_STAGE_MANAGER_PANEL_H
#define USD_STAGE_MANAGER_PANEL_H

#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/tree.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/editor_file_dialog.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

// Forward declaration
class USDPlugin;

class UsdStageManagerPanel : public VBoxContainer {
    GDCLASS(UsdStageManagerPanel, VBoxContainer);

private:
    Tree *stage_tree = nullptr;
    Button *open_usd_button = nullptr;
    Button *create_group_button = nullptr;
    Button *update_scene_button = nullptr;
    Button *refresh_button = nullptr;
    Label *info_label = nullptr;
    Timer *update_timer = nullptr;

    // File dialog for opening USD files
    EditorFileDialog *open_file_dialog = nullptr;

    // Dialog for creating new group mapping
    AcceptDialog *create_group_dialog = nullptr;
    LineEdit *group_name_input = nullptr;

    // Selected stage info
    uint64_t selected_stage_id = 0;
    String selected_file_path;

    // Reference to plugin for import operations
    USDPlugin* plugin = nullptr;

    void _update_stage_list();
    void _on_stage_selected();
    void _on_open_usd_pressed();
    void _on_usd_file_selected(const String &p_file_path);
    void _on_create_group_pressed();
    void _on_update_scene_pressed();
    void _on_refresh_pressed();
    void _on_update_timer_timeout();
    void _on_create_group_confirmed();

    // Helper to get status icon/text based on stage state
    String _get_status_icon(bool has_mapping, bool needs_update);
    String _get_status_description(bool has_mapping, bool needs_update);

protected:
    static void _bind_methods();

public:
    UsdStageManagerPanel();
    ~UsdStageManagerPanel();

    virtual void _ready() override;

    // Set plugin reference for import operations
    void set_plugin(USDPlugin* p_plugin) { plugin = p_plugin; }
};

} // namespace godot

#endif // USD_STAGE_MANAGER_PANEL_H
