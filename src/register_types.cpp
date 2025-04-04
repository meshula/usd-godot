#include "register_types.h"
#include "usd_plugin.h"
#include "usd_document.h"
#include "usd_export_settings.h"
#include "usd_state.h"

#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

// Register plugin classes
void initialize_godot_usd_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        // Register non-editor classes here
        ClassDB::register_class<UsdDocument>();
        ClassDB::register_class<UsdExportSettings>();
        ClassDB::register_class<UsdState>();
    } else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        // Register editor plugin classes
        ClassDB::register_class<USDPlugin>();
        
        // Register the plugin with the editor
        EditorPlugins::add_by_type<USDPlugin>();
        
        // Register the plugin as a singleton
        Engine::get_singleton()->register_singleton("USDPlugin", memnew(USDPlugin));
    }
}

void uninitialize_godot_usd_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        // Unregister non-editor classes here
        // Note: We don't need to explicitly unregister classes, as they are automatically
        // unregistered when the module is unloaded. This is just for completeness.
    } else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        // Unregister the singleton
        Engine::get_singleton()->unregister_singleton("USDPlugin");
        
        // Unregister editor plugin classes
        EditorPlugins::remove_by_type<USDPlugin>();
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
