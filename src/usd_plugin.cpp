#include "usd_plugin.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

void USDPlugin::_bind_methods() {
    // No methods to bind for now
}

USDPlugin::USDPlugin() {
    // Constructor
}

USDPlugin::~USDPlugin() {
    // Destructor
}

void USDPlugin::_enter_tree() {
    // Called when the plugin is added to the editor
    UtilityFunctions::print("USD Plugin: Enter Tree");
}

void USDPlugin::_exit_tree() {
    // Called when the plugin is removed from the editor
    UtilityFunctions::print("USD Plugin: Exit Tree");
}

bool USDPlugin::_has_main_screen() const {
    // Return true if the plugin needs a main screen
    return false;
}

String USDPlugin::_get_plugin_name() const {
    // Return the name of the plugin
    return "USD";
}

}
