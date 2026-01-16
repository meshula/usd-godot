#ifndef MCP_CONTROL_PANEL_H
#define MCP_CONTROL_PANEL_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/text_edit.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include "mcp_globals.h"

namespace godot {

class McpControlPanel : public VBoxContainer {
    GDCLASS(McpControlPanel, VBoxContainer)

private:
    // UI Elements
    Button* start_stop_button;
    Label* status_label;
    RichTextLabel* operation_log;
    TextEdit* user_notes_field;
    Timer* update_timer;

    // State
    bool mcp_running;
    String notes_file_path;

    // Methods
    void _update_status();
    void _on_start_stop_pressed();
    void _on_update_timer_timeout();
    void _on_copy_log_pressed();
    void _append_log(const String& message);
    void _save_user_notes();
    void _load_user_notes();

protected:
    static void _bind_methods();

public:
    McpControlPanel();
    ~McpControlPanel();

    void _ready() override;
    void _process(double delta) override;

    // Public API for logging operations
    void log_operation(const String& operation, const String& details);
    void set_mcp_running(bool running);
    bool is_mcp_running() const { return mcp_running; }
};

} // namespace godot

#endif // MCP_CONTROL_PANEL_H
