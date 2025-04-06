#include "usd_plugin.h"
#include "usd_document.h"
#include "usd_export_settings.h"
#include "usd_mesh_export_helper.h"
#include "usd_mesh_import_helper.h"
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
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/string.hpp>

#include <pxr/base/plug/registry.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/gf/vec3f.h>

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
    ClassDB::bind_method(D_METHOD("_popup_usd_import_dialog"), &USDPlugin::_popup_usd_import_dialog);
    ClassDB::bind_method(D_METHOD("_import_usd_file", "file_path"), &USDPlugin::_import_usd_file);
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
    
    if (_import_file_dialog) {
        _import_file_dialog->queue_free();
        _import_file_dialog = nullptr;
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
    
    // Add the "Import USD" button to the editor's toolbar
    Button *import_button = memnew(Button);
    import_button->set_text("Import USD");
    import_button->set_tooltip_text("Import a USD file into the current scene");
    import_button->connect("pressed", Callable(this, "_popup_usd_import_dialog"));
    
    // Add the button to the editor's toolbar
    add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, import_button);
    UtilityFunctions::print("USD Plugin: Added Import USD button to toolbar");
    
    if (!_file_dialog) {
        // Set up the export file dialog
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
    
    if (!_import_file_dialog) {
        // Set up the import file dialog
        _import_file_dialog = memnew(EditorFileDialog);
        _import_file_dialog->connect("file_selected", Callable(this, "_import_usd_file"));
        _import_file_dialog->set_title("Import USD Scene");
        _import_file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
        _import_file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
        _import_file_dialog->clear_filters();
        _import_file_dialog->add_filter("*.usd");
        _import_file_dialog->add_filter("*.usda");
        _import_file_dialog->add_filter("*.usdc");
    }

    // Add the file dialogs to the editor
    EditorInterface::get_singleton()->get_base_control()->add_child(_file_dialog);
    EditorInterface::get_singleton()->get_base_control()->add_child(_import_file_dialog);
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
    
    // The import button is automatically removed when the plugin is removed
    
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

void USDPlugin::_popup_usd_import_dialog() {
    // This method is called when the user selects "USD Scene..." from the File -> Import menu
    
    // Get the editor interface
    EditorInterface *editor = EditorInterface::get_singleton();
    if (!editor) {
        UtilityFunctions::printerr("USD Import: Failed to get EditorInterface singleton");
        return;
    }
    
    // Show the file dialog
    _import_file_dialog->popup_centered_ratio();
    
    UtilityFunctions::print("USD Import: Showing import dialog");
}

void USDPlugin::_import_usd_file(const String &p_file_path) {
    // This method is called when the user selects a file path in the import dialog
    
    // Get the editor interface
    EditorInterface *editor = EditorInterface::get_singleton();
    if (!editor) {
        UtilityFunctions::printerr("USD Import: Failed to get EditorInterface singleton");
        return;
    }
    
    UtilityFunctions::print("USD Import: Importing USD file from ", p_file_path);
    
    try {
        // Create a new USD state
        Ref<UsdState> state;
        state.instantiate();
        
        // Open the USD stage
        UsdStageRefPtr stage = UsdStage::Open(p_file_path.utf8().get_data());
        if (!stage) {
            UtilityFunctions::printerr("USD Import: Failed to open USD stage from ", p_file_path);
            return;
        }
        
        // Store the stage in the state
        state->set_stage(stage);
        
        // Get the default prim
        UsdPrim defaultPrim = stage->GetDefaultPrim();
        if (!defaultPrim) {
            // If there's no default prim, use the pseudo-root
            defaultPrim = stage->GetPseudoRoot();
        }
        
        // Print some information about the USD file
        UtilityFunctions::print("USD Import: Default prim: ", String(defaultPrim.GetName().GetText()));
        UtilityFunctions::print("USD Import: Stage start timeCode: ", stage->GetStartTimeCode());
        UtilityFunctions::print("USD Import: Stage end timeCode: ", stage->GetEndTimeCode());
        
        // Print the prim hierarchy (for debugging)
        UtilityFunctions::print("USD Import: Prim hierarchy:");
        _print_prim_hierarchy(defaultPrim, 0);
        
        // Create a root node for the imported scene
        Node3D *root = memnew(Node3D);
        root->set_name(p_file_path.get_file().get_basename());
        
        // Convert USD prims to Godot nodes
        // Pass the root node as both the parent and the scene root
        _convert_prim_to_node(defaultPrim, root, root);
        
        // Print the node hierarchy for debugging
        UtilityFunctions::print("USD Import: Node hierarchy before packing:");
        _print_node_hierarchy(root, 0);
        
        // Create a scene with the root node
        Ref<PackedScene> scene;
        scene.instantiate();
        
        // Pack the scene
        Error pack_err = scene->pack(root);
        if (pack_err != OK) {
            UtilityFunctions::printerr("USD Import: Failed to pack scene: ", pack_err);
            root->queue_free(); // Clean up the root node
            return;
        }
        
        // Save the scene to a file
        String temp_scene_path = "res://" + p_file_path.get_file().get_basename() + ".tscn";
        Error save_err = ResourceSaver::get_singleton()->save(scene, temp_scene_path);
        if (save_err != OK) {
            UtilityFunctions::printerr("USD Import: Failed to save scene to ", temp_scene_path, " Error: ", save_err);
            root->queue_free(); // Clean up the root node
            return;
        }
        
        // Open the scene in the editor
        editor->open_scene_from_path(temp_scene_path);
        
        UtilityFunctions::print("USD Import: Successfully imported USD file from ", p_file_path);
        UtilityFunctions::print("USD Import: Saved scene to ", temp_scene_path);
        
    } catch (const std::exception& e) {
        UtilityFunctions::printerr("USD Import: Exception occurred: ", e.what());
    }
}

// Helper method to extract and apply transform from a USD prim to a Godot node
bool USDPlugin::_apply_transform_from_usd_prim(const UsdPrim &p_prim, Node3D *p_node) {
    if (!p_node) {
        return false;
    }
    
    // Extract transform from USD prim
    UsdGeomXform usdXform(p_prim);
    bool reset_xform_stack = false;
    std::vector<UsdGeomXformOp> xform_ops = usdXform.GetOrderedXformOps(&reset_xform_stack);
    
    if (xform_ops.empty()) {
        UtilityFunctions::print("USD Import: No transform found for node: ", p_node->get_name());
        return false;
    }
    
    // Get the transform matrix
    GfMatrix4d usd_matrix;
    bool resetsXformStack = false;
    usdXform.GetLocalTransformation(&usd_matrix, &resetsXformStack);
    
    // Convert USD matrix to Godot transform
    Transform3D transform;
    
    // Extract basis (rotation and scale)
    Basis basis(
        Vector3(usd_matrix[0][0], usd_matrix[0][1], usd_matrix[0][2]),
        Vector3(usd_matrix[1][0], usd_matrix[1][1], usd_matrix[1][2]),
        Vector3(usd_matrix[2][0], usd_matrix[2][1], usd_matrix[2][2])
    );
    
    // Extract translation
    Vector3 origin(usd_matrix[3][0], usd_matrix[3][1], usd_matrix[3][2]);
    
    // Set the transform
    transform.set_basis(basis);
    transform.set_origin(origin);
    
    // Apply the transform to the node
    p_node->set_transform(transform);
    
    //UtilityFunctions::print("USD Import: Applied transform to node: ", p_node->get_name());
    return true;
}

// Helper method to convert a USD prim to a Godot node
Node *USDPlugin::_convert_prim_to_node(const UsdPrim &p_prim, Node *p_parent, Node *p_scene_root) {
    // Skip the pseudo-root
    if (p_prim.IsPseudoRoot()) {
        // Process children
        for (UsdPrim child : p_prim.GetChildren()) {
            _convert_prim_to_node(child, p_parent, p_scene_root ? p_scene_root : p_parent);
        }
        return p_parent;
    }
    
    // Get the prim type and name
    String prim_type = String(p_prim.GetTypeName().GetText());
    String prim_name = String(p_prim.GetName().GetText());
    
    bool prim_is_mesh = !!pxr::UsdGeomGprim(p_prim);

    // Debug output
    //UtilityFunctions::print("USD Import: Converting prim: ", prim_name, " with type: ", prim_type);
    
    // Create a node based on the prim type
    Node *node = nullptr;
    
    if (prim_type == "Xform") {
        // Create a Node3D for Xform prims
        Node3D *xform = memnew(Node3D);
        xform->set_name(prim_name);
        
        // Apply transform from USD prim
        _apply_transform_from_usd_prim(p_prim, xform);
        
        node = xform;
    } else if (prim_type == "Scope") {
        // Create a Node3D for Scope prims (organizational)
        Node3D *scope = memnew(Node3D);
        scope->set_name(prim_name);
        
        // Apply transform from USD prim
        _apply_transform_from_usd_prim(p_prim, scope);
        
        //UtilityFunctions::print("USD Import: Created Scope node: ", prim_name);
        node = scope;
    } else if (prim_type == "Material" || prim_type == "Shader") {
        // Skip materials and shaders for now
        // In a full implementation, we would create materials and shaders
        // UtilityFunctions::print("USD Import: Skipping Material/Shader: ", prim_name);
    } else if (prim_is_mesh) {
        // Create a MeshInstance3D with a BoxMesh for Cube prims
        MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
        mesh_instance->set_name(prim_name);
        
        UsdMeshImportHelper helper;
        Ref<Mesh> box_mesh = helper.import_mesh_from_prim(p_prim);

        mesh_instance->set_mesh(box_mesh);
        auto mat = helper.create_material(p_prim);
                    
        // Apply the material to the mesh
        if (mat.is_valid()) {
            mesh_instance->set_surface_override_material(0, mat);
        }

        // Apply transform from USD prim
        _apply_transform_from_usd_prim(p_prim, mesh_instance);
        
        //UtilityFunctions::print("USD Import: Created ", prim_type, " node: ", prim_name);
        
        node = mesh_instance;
    } else {
        // For empty or unknown prim types, create a Node3D
        // This handles cases like "Shapes" and "Materials" which have empty type names
        Node3D *generic = memnew(Node3D);
        generic->set_name(prim_name);
        
        // Apply transform from USD prim
        _apply_transform_from_usd_prim(p_prim, generic);
        
        UtilityFunctions::print("USD Import: Created generic node for type: ", prim_type, " prim: ", prim_name);
        node = generic;
    }
    
    // Add the node to the parent
    if (node) {
        p_parent->add_child(node);
        
        // Set the owner for proper serialization
        if (p_scene_root) {
            node->set_owner(p_scene_root);
        } else {
            // If no scene root is provided, use the node itself as the owner
            node->set_owner(p_parent->get_owner());
        }
        
        // Process children
        for (UsdPrim child : p_prim.GetChildren()) {
            _convert_prim_to_node(child, node, p_scene_root);
        }
    }
    
    return node;
}

// Helper method to print the prim hierarchy
void USDPlugin::_print_prim_hierarchy(const UsdPrim &p_prim, int p_indent) {
    // Create an indentation string
    String indent = "";
    for (int i = 0; i < p_indent; i++) {
        indent += "  ";
    }
    
    // Print the prim name and type
    UtilityFunctions::print(indent, "- ", String(p_prim.GetName().GetText()), " (", String(p_prim.GetTypeName().GetText()), ")");
    
    // Recursively print children
    for (UsdPrim child : p_prim.GetChildren()) {
        _print_prim_hierarchy(child, p_indent + 1);
    }
}

// Helper method to print the node hierarchy
void USDPlugin::_print_node_hierarchy(Node *p_node, int p_indent) {
    if (!p_node) {
        return;
    }
    
    // Create an indentation string
    String indent = "";
    for (int i = 0; i < p_indent; i++) {
        indent += "  ";
    }
    
    // Print the node name, class, and owner
    String owner_info = p_node->get_owner() ? String(" (owner: ") + p_node->get_owner()->get_name() + ")" : " (no owner)";
    UtilityFunctions::print(indent, "- ", p_node->get_name(), " [", p_node->get_class(), "]", owner_info);
    
    // Recursively print children
    for (int i = 0; i < p_node->get_child_count(); i++) {
        _print_node_hierarchy(p_node->get_child(i), p_indent + 1);
    }
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
