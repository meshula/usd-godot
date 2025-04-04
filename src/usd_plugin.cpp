#include "usd_plugin.h"
#include "usd_document.h"
#include "usd_export_settings.h"
#include "usd_state.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/popup_menu.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/text_mesh.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/string.hpp>

#include <pxr/base/plug/registry.h>

PXR_NAMESPACE_USING_DIRECTIVE

// For setenv
#if defined(_WIN32)
#include <stdlib.h>
#define setenv(name, value, overwrite) _putenv_s(name, value)
#else
#include <stdlib.h>
#endif

namespace godot {

void USDPlugin::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_hello_button_pressed"), &USDPlugin::_on_hello_button_pressed);
    ClassDB::bind_method(D_METHOD("_popup_usd_export_dialog"), &USDPlugin::_popup_usd_export_dialog);
    ClassDB::bind_method(D_METHOD("_export_scene_as_usd", "file_path"), &USDPlugin::_export_scene_as_usd);
}

USDPlugin::USDPlugin() {
    // Constructor
    _usd_document.instantiate();
    _export_settings.instantiate();
    
    // Generate the property list for the export settings
    _export_settings->generate_property_list(_usd_document);
    
    // Set the PXR_PLUGINPATH_NAME environment variable to include the Godot scene's root directory
    // We'll set this in _enter_tree when we have access to the editor interface
}

USDPlugin::~USDPlugin() {
    // Destructor
    if (_file_dialog) {
        _file_dialog->queue_free();
        _file_dialog = nullptr;
    }
    
    if (_settings_inspector) {
        _settings_inspector->queue_free();
        _settings_inspector = nullptr;
    }
}

void USDPlugin::_enter_tree() {
    // Called when the plugin is added to the editor
    UtilityFunctions::print("USD Plugin: Enter Tree");
    
    // Get the project root directory
    // In Godot, we can use the OS class to get the current directory
    String project_root = ProjectSettings::get_singleton()->globalize_path("res://");
    // if project root does not end with a slash, add one
    if (!project_root.ends_with("/")) {
        project_root += "/";
    }

    std::vector<std::string> pluginPaths;
    std::string pluginPath = project_root.utf8().get_data();
    pluginPaths.push_back(std::string(pluginPath + "lib/lib/usd"));
    pluginPaths.push_back(std::string(pluginPath + "lib/plugin"));


    // use USD's plugin registry to register the plugins
    // get the plugin registry singleton and call RegisterPlugins
    PlugRegistry::GetInstance().RegisterPlugins(pluginPaths);
    
    // Create the "Hello USD" button
    hello_button = memnew(Button);
    hello_button->set_text("Hello USD");
    hello_button->set_tooltip_text("Create a 'Hello USD' text node in the scene");
    hello_button->connect("pressed", Callable(this, "_on_hello_button_pressed"));
    
    // Add the button to the editor's toolbar
    add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, hello_button);
    UtilityFunctions::print("USD Plugin: Added Hello USD button to toolbar");
    
    // Add the USD export menu item to the Scene -> Export menu
    PopupMenu *export_menu = get_export_as_menu();
    if (export_menu) {
        int idx = export_menu->get_item_count();
        export_menu->add_item("USD Scene...");
        export_menu->set_item_metadata(idx, Callable(this, "_popup_usd_export_dialog"));
        UtilityFunctions::print("USD Plugin: Added USD Scene export menu item");
    } else {
        UtilityFunctions::printerr("USD Plugin: Failed to get export menu");
    }
    
    if (!_file_dialog) {
        // Set up the file dialog
        _file_dialog = memnew(EditorFileDialog);
        _file_dialog->connect("file_selected", Callable(this, "_export_scene_as_usd"));
        _file_dialog->set_title("Export Scene to USD");
        _file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
        _file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
        _file_dialog->clear_filters();
        _file_dialog->add_filter("*.usd");
        _file_dialog->add_filter("*.usda");
        _file_dialog->add_filter("*.usdc");
        
        // Set up the export settings menu
        _settings_inspector = memnew(EditorInspector);
        _settings_inspector->set_custom_minimum_size(Size2(350, 300));
        _file_dialog->add_side_menu(_settings_inspector, "Export Settings:");
    }

    // Add the file dialog to the editor
    EditorInterface::get_singleton()->get_base_control()->add_child(_file_dialog);
}

void USDPlugin::_exit_tree() {
    // Called when the plugin is removed from the editor
    if (hello_button) {
        hello_button->queue_free();
        hello_button = nullptr;
    }
    
    // Remove the export menu item
    PopupMenu *export_menu = get_export_as_menu();
    if (export_menu) {
        for (int i = 0; i < export_menu->get_item_count(); i++) {
            Variant metadata = export_menu->get_item_metadata(i);
            if (metadata.get_type() == Variant::CALLABLE) {
                Callable callable = metadata;
                if (callable.get_object() == this) {
                    export_menu->remove_item(i);
                    break;
                }
            }
        }
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

void USDPlugin::_popup_usd_export_dialog() {
    // This method is called when the user selects "USD Scene..." from the Scene -> Export menu
    
    // Get the editor interface
    EditorInterface *editor = EditorInterface::get_singleton();
    if (!editor) {
        UtilityFunctions::printerr("USD Export: Failed to get EditorInterface singleton");
        return;
    }
    
    // Get the edited scene root
    Node *edited_scene_root = editor->get_edited_scene_root();
    if (!edited_scene_root) {
        UtilityFunctions::print("USD Export: No scene is currently being edited");
        editor->get_base_control()->add_child(_file_dialog);
        editor->get_base_control()->remove_child(_file_dialog);
        return;
    }
    
    // Set the file dialog's file name to the scene name
    String filename = String(edited_scene_root->get_scene_file_path().get_file().get_basename());
    if (filename.is_empty()) {
        filename = edited_scene_root->get_name();
    }
    _file_dialog->set_current_file(filename + String(".usd"));
    
    // Generate and refresh the export settings
    _export_settings->generate_property_list(_usd_document, edited_scene_root);
    
    // Note: In a full implementation, we would update the inspector to show the export settings
    // But for now, we'll just print the settings
    UtilityFunctions::print("USD Export: Using export settings:");
    UtilityFunctions::print("  - Export materials: ", _export_settings->get_export_materials());
    UtilityFunctions::print("  - Export animations: ", _export_settings->get_export_animations());
    UtilityFunctions::print("  - Export cameras: ", _export_settings->get_export_cameras());
    UtilityFunctions::print("  - Export lights: ", _export_settings->get_export_lights());
    UtilityFunctions::print("  - Export meshes: ", _export_settings->get_export_meshes());
    UtilityFunctions::print("  - Export textures: ", _export_settings->get_export_textures());
    UtilityFunctions::print("  - Copyright: ", _export_settings->get_copyright());
    UtilityFunctions::print("  - Bake FPS: ", _export_settings->get_bake_fps());
    UtilityFunctions::print("  - Use binary format: ", _export_settings->get_use_binary_format());
    UtilityFunctions::print("  - Flatten stage: ", _export_settings->get_flatten_stage());
    UtilityFunctions::print("  - Export as single layer: ", _export_settings->get_export_as_single_layer());
    UtilityFunctions::print("  - Export with references: ", _export_settings->get_export_with_references());
    
    // Show the file dialog
    _file_dialog->popup_centered_ratio();
    
    UtilityFunctions::print("USD Export: Showing export dialog");
}

void USDPlugin::_export_scene_as_usd(const String &p_file_path) {
    // This method is called when the user selects a file path in the export dialog
    
    // Get the editor interface
    EditorInterface *editor = EditorInterface::get_singleton();
    if (!editor) {
        UtilityFunctions::printerr("USD Export: Failed to get EditorInterface singleton");
        return;
    }
    
    // Get the edited scene root
    Node *edited_scene_root = editor->get_edited_scene_root();
    if (!edited_scene_root) {
        UtilityFunctions::printerr("USD Export: No scene is currently being edited");
        return;
    }
    
    UtilityFunctions::print("USD Export: Exporting scene to ", p_file_path);
    
    // Create a new USD state
    Ref<UsdState> state;
    state.instantiate();
    state->set_copyright(_export_settings->get_copyright());
    state->set_bake_fps(_export_settings->get_bake_fps());
    
    // Export the scene
    Error err = _usd_document->append_from_scene(edited_scene_root, state);
    if (err != OK) {
        UtilityFunctions::printerr("USD Export: Failed to append scene to USD document");
        return;
    }
    
    // Write the USD file
    err = _usd_document->write_to_filesystem(state, p_file_path);
    if (err != OK) {
        UtilityFunctions::printerr("USD Export: Failed to write USD document to filesystem");
        return;
    }
    
    UtilityFunctions::print("USD Export: Successfully exported scene to ", p_file_path);
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
