#ifndef USD_REGISTER_TYPES_H
#define USD_REGISTER_TYPES_H

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

// Function to register plugin classes
void initialize_godot_usd_module(ModuleInitializationLevel p_level);
void uninitialize_godot_usd_module(ModuleInitializationLevel p_level);

#endif // USD_REGISTER_TYPES_H
