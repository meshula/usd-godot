#include "mcp_control_panel.h"
#include "mcp_server.h"
#include "mcp_http_server.h"
#include "mcp_globals.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>

namespace godot {

void McpControlPanel::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_start_stop_pressed"), &McpControlPanel::_on_start_stop_pressed);
    ClassDB::bind_method(D_METHOD("_on_update_timer_timeout"), &McpControlPanel::_on_update_timer_timeout);
    ClassDB::bind_method(D_METHOD("_on_copy_log_pressed"), &McpControlPanel::_on_copy_log_pressed);
    ClassDB::bind_method(D_METHOD("_save_user_notes"), &McpControlPanel::_save_user_notes);
    ClassDB::bind_method(D_METHOD("log_operation", "operation", "details"), &McpControlPanel::log_operation);
    ClassDB::bind_method(D_METHOD("set_mcp_running", "running"), &McpControlPanel::set_mcp_running);
}

McpControlPanel::McpControlPanel() : mcp_running(false) {
    set_name("MCP Control Panel");

    // Get user:// path for notes
    notes_file_path = "user://mcp_user_notes.txt";
}

McpControlPanel::~McpControlPanel() {
}

void McpControlPanel::_ready() {
    // Title
    Label* title = memnew(Label);
    title->set_text("USD MCP Control Panel");
    title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    title->add_theme_font_size_override("font_size", 16);
    add_child(title);

    // Status Section
    HBoxContainer* status_container = memnew(HBoxContainer);
    add_child(status_container);

    Label* status_title = memnew(Label);
    status_title->set_text("Status:");
    status_container->add_child(status_title);

    status_label = memnew(Label);
    status_label->set_text("Not Running");
    status_label->add_theme_color_override("font_color", Color(1, 0, 0)); // Red
    status_container->add_child(status_label);

    // Start/Stop Button
    start_stop_button = memnew(Button);
    start_stop_button->set_text("Start MCP Server");
    start_stop_button->connect("pressed", Callable(this, "_on_start_stop_pressed"));
    add_child(start_stop_button);

    // Operation Log Section
    HBoxContainer* log_header = memnew(HBoxContainer);
    add_child(log_header);

    Label* log_title = memnew(Label);
    log_title->set_text("Operation Log:");
    log_header->add_child(log_title);

    Button* copy_log_button = memnew(Button);
    copy_log_button->set_text("Copy Log");
    copy_log_button->set_tooltip_text("Copy operation log to clipboard");
    copy_log_button->connect("pressed", Callable(this, "_on_copy_log_pressed"));
    log_header->add_child(copy_log_button);

    operation_log = memnew(RichTextLabel);
    operation_log->set_custom_minimum_size(Vector2(0, 200));
    operation_log->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    operation_log->set_use_bbcode(true);
    add_child(operation_log);

    // User Notes Section
    Label* notes_title = memnew(Label);
    notes_title->set_text("Notes for LLM (type PAUSE to interrupt):");
    add_child(notes_title);

    user_notes_field = memnew(TextEdit);
    user_notes_field->set_custom_minimum_size(Vector2(0, 100));
    user_notes_field->set_placeholder("Leave instructions for the LLM here...\nExample: 'PAUSE - Check with user before proceeding'\nExample: 'Focus on the /Character prim next'");
    add_child(user_notes_field);

    Button* save_notes_button = memnew(Button);
    save_notes_button->set_text("Save Notes");
    save_notes_button->connect("pressed", Callable(this, "_save_user_notes"));
    add_child(save_notes_button);

    // Update timer (checks MCP status every second)
    update_timer = memnew(Timer);
    update_timer->set_wait_time(1.0);
    update_timer->set_autostart(true);
    update_timer->connect("timeout", Callable(this, "_on_update_timer_timeout"));
    add_child(update_timer);

    // Load existing notes
    _load_user_notes();

    // Initial log message
    _append_log("[color=gray]MCP Control Panel initialized[/color]");
    _append_log("[color=gray]Ready to start MCP server[/color]");
}

void McpControlPanel::_process(double delta) {
    // Optional: Could add real-time updates here if needed
}

void McpControlPanel::_update_status() {
    mcp::McpServer* server = usd_godot::get_mcp_server_instance();
    mcp::McpHttpServer* http_server = usd_godot::get_mcp_http_server_instance();

    // Check if HTTP server is running
    if (http_server && http_server->is_running()) {
        int port = http_server->get_port();
        status_label->set_text("✓ HTTP Server Running on port " + String::num_int64(port));
        status_label->add_theme_color_override("font_color", Color(0, 1, 0)); // Green
        start_stop_button->set_text("Stop MCP Server");
        start_stop_button->set_disabled(false);
        mcp_running = true;
    } else if (server && server->is_running()) {
        // Stdio server is running (headless mode)
        status_label->set_text("✓ Stdio Server Running");
        status_label->add_theme_color_override("font_color", Color(0, 1, 0)); // Green
        start_stop_button->set_text("Stop MCP Server");
        start_stop_button->set_disabled(false);
        mcp_running = true;
    } else if (server) {
        status_label->set_text("● Ready (not started)");
        status_label->add_theme_color_override("font_color", Color(1, 0.5, 0)); // Orange
        start_stop_button->set_text("Start MCP Server");
        start_stop_button->set_disabled(false);
        mcp_running = false;
    } else {
        status_label->set_text("✗ Not Available (use --mcp flag)");
        status_label->add_theme_color_override("font_color", Color(0.5, 0.5, 0.5)); // Gray
        start_stop_button->set_text("Start MCP Server");
        start_stop_button->set_disabled(true);
        mcp_running = false;
    }
}

void McpControlPanel::_on_start_stop_pressed() {
    mcp::McpServer* server = usd_godot::get_mcp_server_instance();
    mcp::McpHttpServer* http_server = usd_godot::get_mcp_http_server_instance();

    if (!server) {
        _append_log("[color=red]ERROR: MCP server not initialized[/color]");
        _append_log("[color=orange]Start Godot with: --mcp --path /path/to/project[/color]");
        return;
    }

    // Check if either server is running
    bool any_running = (http_server && http_server->is_running()) || server->is_running();

    if (any_running) {
        _append_log("[color=yellow]Stopping MCP servers...[/color]");

        // Stop HTTP server if running
        if (http_server && http_server->is_running()) {
            http_server->stop();
            _append_log("[color=red]HTTP server stopped[/color]");
        }

        // Stop stdio server if running
        if (server->is_running()) {
            server->stop();
            _append_log("[color=red]Stdio server stopped[/color]");
        }

        set_mcp_running(false);
    } else {
        // Start HTTP server (preferred for GUI mode)
        _append_log("[color=yellow]Starting HTTP MCP server...[/color]");

        if (!http_server) {
            _append_log("[color=red]HTTP server not available - try starting Godot with --mcp (without --headless)[/color]");
            return;
        }

        // Set up logging callbacks BEFORE starting (or if already started)
        // This ensures callbacks are set whether we start it now or it's already running
        http_server->set_log_callback([this](const std::string& operation, const std::string& details) {
            call_deferred("log_operation", String(operation.c_str()), String(details.c_str()));
        });

        server->set_log_callback([this](const std::string& operation, const std::string& details) {
            call_deferred("log_operation", String(operation.c_str()), String(details.c_str()));
        });

        // Check if already running (auto-started at initialization)
        if (http_server->is_running()) {
            set_mcp_running(true);
            _append_log("[color=green]✓ HTTP MCP server already running on http://127.0.0.1:3000[/color]");
            _append_log("[color=cyan]Logging callbacks connected[/color]");
        } else if (http_server->start(3000)) {
            set_mcp_running(true);
            _append_log("[color=green]✓ HTTP MCP server started on http://127.0.0.1:3000[/color]");
            _append_log("[color=cyan]Connect from Claude Code using this URL[/color]");
        } else {
            _append_log("[color=red]✗ Failed to start HTTP MCP server[/color]");
        }
    }
}

void McpControlPanel::_on_update_timer_timeout() {
    // Check if MCP server is actually running
    mcp::McpServer* server = usd_godot::get_mcp_server_instance();
    if (server) {
        bool server_running = server->is_running();
        if (server_running != mcp_running) {
            set_mcp_running(server_running);
        }
    } else {
        if (mcp_running) {
            set_mcp_running(false);
        }
    }
    _update_status();
}

void McpControlPanel::_on_copy_log_pressed() {
    if (!operation_log) return;

    // Get all log text
    String log_text = operation_log->get_parsed_text();

    // Copy to clipboard using DisplayServer
    DisplayServer::get_singleton()->clipboard_set(log_text);

    // Show confirmation
    _append_log("[color=cyan]Log copied to clipboard[/color]");
    UtilityFunctions::print("MCP Control Panel: Log copied to clipboard (", log_text.length(), " characters)");
}

void McpControlPanel::_append_log(const String& message) {
    if (!operation_log) return;

    // Get current time
    Dictionary time = Time::get_singleton()->get_time_dict_from_system();
    String timestamp = String("[") + String::num_int64(time["hour"]) + ":" +
                      String::num_int64(time["minute"]).pad_zeros(2) + ":" +
                      String::num_int64(time["second"]).pad_zeros(2) + "]";

    // Append to log
    String current_text = operation_log->get_text();
    operation_log->append_text(timestamp + " " + message + "\n");

    // Auto-scroll to bottom
    operation_log->scroll_to_line(operation_log->get_line_count());
}

void McpControlPanel::_save_user_notes() {
    if (!user_notes_field) return;

    String notes = user_notes_field->get_text();

    // Update global notes for MCP server to include in responses
    usd_godot::set_user_notes(notes.utf8().get_data());

    Ref<FileAccess> file = FileAccess::open(notes_file_path, FileAccess::WRITE);

    if (file.is_valid()) {
        file->store_string(notes);
        file->close();
        _append_log("[color=cyan]User notes saved and synced to MCP[/color]");
        UtilityFunctions::print("MCP Control Panel: User notes saved to ", notes_file_path);
    } else {
        _append_log("[color=red]Failed to save user notes[/color]");
        UtilityFunctions::printerr("MCP Control Panel: Failed to save notes to ", notes_file_path);
    }
}

void McpControlPanel::_load_user_notes() {
    if (!user_notes_field) return;

    Ref<FileAccess> file = FileAccess::open(notes_file_path, FileAccess::READ);

    if (file.is_valid()) {
        String notes = file->get_as_text();
        file->close();
        user_notes_field->set_text(notes);

        // Sync to global for MCP server
        usd_godot::set_user_notes(notes.utf8().get_data());

        _append_log("[color=cyan]User notes loaded[/color]");
    } else {
        // No existing notes file, that's okay
        UtilityFunctions::print("MCP Control Panel: No existing notes file");
    }
}

void McpControlPanel::log_operation(const String& operation, const String& details) {
    String log_msg = "[color=lightblue][b]" + operation + "[/b][/color]";
    if (!details.is_empty()) {
        log_msg += " - " + details;
    }
    _append_log(log_msg);
}

void McpControlPanel::set_mcp_running(bool running) {
    mcp_running = running;
    _update_status();
}

} // namespace godot
