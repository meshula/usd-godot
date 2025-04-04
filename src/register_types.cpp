#include "register_types.h"
#include "usd_plugin.h"

#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

// Register plugin classes
void initialize_godot_usd_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    
    ClassDB::register_class<USDPlugin>();
}

void uninitialize_godot_usd_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    
    // Nothing to do here for now
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
