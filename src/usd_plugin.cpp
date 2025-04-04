#include "usd_plugin.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/text_mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>

namespace godot {

void USDPlugin::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_hello_button_pressed"), &USDPlugin::_on_hello_button_pressed);
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
    
    // Create the "Hello USD" button
    hello_button = memnew(Button);
    hello_button->set_text("Hello USD");
    hello_button->set_tooltip_text("Create a 'Hello USD' text node in the scene");
    hello_button->connect("pressed", Callable(this, "_on_hello_button_pressed"));
    
    // Add the button to the editor's toolbar
    add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, hello_button);
    UtilityFunctions::print("USD Plugin: Added Hello USD button to toolbar");
}

void USDPlugin::_exit_tree() {
    // Called when the plugin is removed from the editor
    if (hello_button) {
        hello_button->queue_free();
        hello_button = nullptr;
    }
    UtilityFunctions::print("USD Plugin: Exit Tree");
}

void USDPlugin::_on_hello_button_pressed() {
    UtilityFunctions::print("Hello USD button pressed!");
    
    // Get the editor interface
    EditorInterface *editor = EditorInterface::get_singleton();
    if (!editor) {
        UtilityFunctions::printerr("Failed to get EditorInterface singleton");
        return;
    }
    
    // Get the edited scene root
    Node *edited_scene_root = editor->get_edited_scene_root();
    if (!edited_scene_root) {
        UtilityFunctions::printerr("No scene is currently being edited");
        return;
    }
    
    // Create a new TextMesh
    Ref<TextMesh> text_mesh;
    text_mesh.instantiate();
    text_mesh->set_text("Hello USD");
    
    // Create a MeshInstance3D to display the text mesh
    MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
    mesh_instance->set_name("HelloUSD");
    mesh_instance->set_mesh(text_mesh);
    
    // Add the mesh instance to the scene
    edited_scene_root->add_child(mesh_instance);
    mesh_instance->set_owner(edited_scene_root);
    
    UtilityFunctions::print("Added 'Hello USD' text node to the scene");
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
