#include "register_types.h"
#include "usd_plugin.h"
#include "usd_document.h"
#include "usd_export_settings.h"
#include "usd_state.h"
#include "usd_stage_proxy.h"
#include "usd_prim_proxy.h"

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

// Register plugin classes
void initialize_godot_usd_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        // Register USD plugins before anything else that uses USD
        register_usd_plugins();

        // Register non-editor classes here
        ClassDB::register_class<UsdDocument>();
        ClassDB::register_class<UsdExportSettings>();
        ClassDB::register_class<UsdState>();
        ClassDB::register_class<UsdStageProxy>();
        ClassDB::register_class<UsdPrimProxy>();
    } else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        // Register editor plugin classes - only in editor mode
        if (Engine::get_singleton()->is_editor_hint()) {
            ClassDB::register_class<USDPlugin>();

            // Register the plugin with the editor
            EditorPlugins::add_by_type<USDPlugin>();
        }
    }
}

void uninitialize_godot_usd_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
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
