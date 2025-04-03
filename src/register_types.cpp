#include "register_types.h"
#include "usd_plugin.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

// GDExtension initialization function
extern "C" {
GDExtensionBool GDE_EXPORT godot_usd_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    
    init_obj.register_initializer(initialize_godot_usd_module);
    init_obj.register_terminator(uninitialize_godot_usd_module);
    
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    
    return init_obj.init();
}
}

// Plugin initialization
void initialize_godot_usd_module() {
    ClassDB::register_class<USDPlugin>();
    
    // Log a message to confirm initialization
    UtilityFunctions::print("Godot-USD plugin initialized!");
}

// Plugin cleanup
void uninitialize_godot_usd_module() {
    // Log a message to confirm cleanup
    UtilityFunctions::print("Godot-USD plugin uninitialized!");
}
