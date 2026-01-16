#include "register_types.h"
#include "usd_plugin.h"
#include "usd_document.h"
#include "usd_export_settings.h"
#include "usd_state.h"
#include "usd_stage_proxy.h"
#include "usd_prim_proxy.h"
#include "mcp_server.h"
#include "mcp_http_server.h"
#include "mcp_control_panel.h"

#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

// USD includes for plugin registration
#include <pxr/base/plug/registry.h>
#include <pxr/base/plug/plugin.h>
#include <pxr/base/arch/fileSystem.h>
#include <pxr/base/arch/symbols.h>

using namespace godot;

// Flag to track if USD plugins have been registered
static bool s_usd_plugins_registered = false;

// Global MCP server instances
static mcp::McpServer* s_mcp_server = nullptr;
static mcp::McpHttpServer* s_mcp_http_server = nullptr;

// Global user notes for real-time LLM communication
static std::string s_user_notes = "";
static std::mutex s_user_notes_mutex;

// Accessors for MCP servers (for control panel)
namespace usd_godot {
    mcp::McpServer* get_mcp_server_instance() {
        return s_mcp_server;
    }

    mcp::McpHttpServer* get_mcp_http_server_instance() {
        return s_mcp_http_server;
    }

    std::string get_user_notes() {
        std::lock_guard<std::mutex> lock(s_user_notes_mutex);
        return s_user_notes;
    }

    void set_user_notes(const std::string& notes) {
        std::lock_guard<std::mutex> lock(s_user_notes_mutex);
        s_user_notes = notes;
    }
}

// Helper function to get the GDExtension library directory
// This returns the path to the directory containing our .dylib/.so/.dll
static std::string get_gdextension_lib_path() {
    // Use USD's ArchGetAddressInfo to find where our library is located
    std::string libraryPath;
    if (ArchGetAddressInfo(
            reinterpret_cast<void*>(&get_gdextension_lib_path),
            &libraryPath, nullptr, nullptr, nullptr)) {
        // libraryPath is the full path to our .dylib
        // Extract the directory portion
        size_t lastSlash = libraryPath.rfind('/');
        if (lastSlash != std::string::npos) {
            return libraryPath.substr(0, lastSlash);
        }
    }
    return "";
}

// Register USD plugins from our bundled plugin directory
static void register_usd_plugins() {
    if (s_usd_plugins_registered) {
        return;
    }

    std::string libPath = get_gdextension_lib_path();
    if (libPath.empty()) {
        UtilityFunctions::printerr("USD: Failed to determine GDExtension library path");
        return;
    }

    // Our USD plugin resources are in lib/usd
    // The library is in lib/libgodot-usd.dylib
    // So USD plugins are at: <libPath>/usd
    std::string pluginPath = libPath + "/usd";

    UtilityFunctions::print(String("USD: Registering plugins from: ") + String(pluginPath.c_str()));

    // Register the plugins with USD's PlugRegistry
    pxr::PlugPluginPtrVector plugins = pxr::PlugRegistry::GetInstance().RegisterPlugins(pluginPath);

    if (plugins.empty()) {
        UtilityFunctions::printerr(String("USD: No plugins found at: ") + String(pluginPath.c_str()));
    } else {
        UtilityFunctions::print(String("USD: Registered ") + String::num_int64(plugins.size()) + String(" plugins"));
        for (const auto& plugin : plugins) {
            UtilityFunctions::print(String("  - ") + String(plugin->GetName().c_str()));
        }
    }

    s_usd_plugins_registered = true;
}

// Check if running in MCP mode (look for --mcp or --interactive flag)
static bool is_mcp_mode() {
    OS* os = OS::get_singleton();
    PackedStringArray args = os->get_cmdline_args();

    for (int i = 0; i < args.size(); i++) {
        String arg = args[i];
        if (arg == "--mcp" || arg == "--interactive") {
            return true;
        }
    }
    return false;
}

// Check if running headless (look for --headless flag)
static bool is_headless_mode() {
    OS* os = OS::get_singleton();
    PackedStringArray args = os->get_cmdline_args();

    for (int i = 0; i < args.size(); i++) {
        String arg = args[i];
        if (arg == "--headless") {
            return true;
        }
    }
    return false;
}

// Register plugin classes
void initialize_godot_usd_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        UtilityFunctions::print("USD-Godot: Initializing at SCENE level");

        // Register USD plugins before anything else that uses USD
        register_usd_plugins();

        // Register non-editor classes here
        ClassDB::register_class<UsdDocument>();
        ClassDB::register_class<UsdExportSettings>();
        ClassDB::register_class<UsdState>();
        ClassDB::register_class<UsdStageProxy>();
        ClassDB::register_class<UsdPrimProxy>();
        ClassDB::register_class<McpControlPanel>();

        UtilityFunctions::print("USD-Godot: Classes registered");

        // Create MCP server instance (used by both stdio and HTTP modes)
        s_mcp_server = new mcp::McpServer();
        s_mcp_server->set_plugin_registered(s_usd_plugins_registered);

        // Start appropriate MCP transport based on mode
        if (is_mcp_mode()) {
            bool headless = is_headless_mode();

            if (headless) {
                // Headless mode: Use stdio transport (for Claude Code to spawn and control)
                UtilityFunctions::print("USD-Godot: Headless MCP mode - starting stdio transport");
                if (s_mcp_server->start()) {
                    UtilityFunctions::print("USD-Godot: MCP stdio server started successfully");
                } else {
                    UtilityFunctions::printerr("USD-Godot: Failed to start MCP stdio server");
                }
            } else {
                // Interactive mode (GUI): Use HTTP transport
                UtilityFunctions::print("USD-Godot: Interactive MCP mode - starting HTTP transport");
                s_mcp_http_server = new mcp::McpHttpServer();
                s_mcp_http_server->set_mcp_server(s_mcp_server);

                if (s_mcp_http_server->start(3000)) {
                    UtilityFunctions::print("USD-Godot: MCP HTTP server started successfully on port 3000");
                } else {
                    UtilityFunctions::printerr("USD-Godot: Failed to start MCP HTTP server");
                }
            }
        } else {
            UtilityFunctions::print("USD-Godot: Not in MCP mode, servers not started");
        }
    } else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        UtilityFunctions::print("USD-Godot: Initializing at EDITOR level");
        // Register editor plugin classes - only in editor mode
        if (Engine::get_singleton()->is_editor_hint()) {
            ClassDB::register_class<USDPlugin>();

            // Register the plugin directly with the editor
            // Note: Direct registration is simpler for GDExtensions in Godot 4.x
            EditorPlugins::add_by_type<USDPlugin>();
        }
    }
}

void uninitialize_godot_usd_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        // Clear callbacks BEFORE stopping servers to prevent crashes during shutdown
        // (callbacks may reference UI objects that are being destroyed)
        if (s_mcp_http_server) {
            s_mcp_http_server->set_log_callback(nullptr);
        }
        if (s_mcp_server) {
            s_mcp_server->set_log_callback(nullptr);
        }

        // Stop MCP servers if they were started
        if (s_mcp_http_server) {
            s_mcp_http_server->stop();
            delete s_mcp_http_server;
            s_mcp_http_server = nullptr;
        }

        if (s_mcp_server) {
            s_mcp_server->stop();
            delete s_mcp_server;
            s_mcp_server = nullptr;
        }
        // Unregister non-editor classes here
        // Note: We don't need to explicitly unregister classes, as they are automatically
        // unregistered when the module is unloaded. This is just for completeness.
    } else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        // Unregister editor plugin classes - only in editor mode
        if (Engine::get_singleton()->is_editor_hint()) {
            EditorPlugins::remove_by_type<USDPlugin>();
        }
    }
}

// GDExtension initialization
extern "C" {
    // Initialization
    GDExtensionBool GDE_EXPORT godot_usd_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
        godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
        
        init_obj.register_initializer(initialize_godot_usd_module);
        init_obj.register_terminator(uninitialize_godot_usd_module);
        
        return init_obj.init();
    }
}
