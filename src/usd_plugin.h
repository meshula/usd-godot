#ifndef USD_PLUGIN_H
#define USD_PLUGIN_H

#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/editor_inspector.hpp>
#include <godot_cpp/classes/editor_file_dialog.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>

// USD headers
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformOp.h>
#include <pxr/usd/usdGeom/cube.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace godot {

// Forward declarations
class UsdDocument;
class UsdExportSettings;
class McpControlPanel;

class USDPlugin : public EditorPlugin {
    GDCLASS(USDPlugin, EditorPlugin);

private:
    Button *hello_button;
    
    // USD Export components
    Ref<UsdDocument> _usd_document;
    Ref<UsdExportSettings> _export_settings;
    EditorInspector *_settings_inspector = nullptr;
    EditorFileDialog *_file_dialog = nullptr;
    
    // USD Import components
    EditorFileDialog *_import_file_dialog = nullptr;

    // MCP Control Panel
    McpControlPanel *_mcp_control_panel = nullptr;
    
    void _popup_usd_export_dialog();
    void _export_scene_as_usd(const String &p_file_path);
    
    void _popup_usd_import_dialog();
    void _import_usd_file(const String &p_file_path);
    
    // Helper method to print the prim hierarchy
    void _print_prim_hierarchy(const UsdPrim &p_prim, int p_indent);
    
    // Helper method to print the node hierarchy
    void _print_node_hierarchy(Node *p_node, int p_indent);
    
    // Helper method to extract and apply transform from a USD prim to a Godot node
    bool _apply_transform_from_usd_prim(const UsdPrim &p_prim, Node3D *p_node);
    
    // Helper method to convert a USD prim to a Godot node
    Node *_convert_prim_to_node(const UsdPrim &p_prim, Node *p_parent, Node *p_scene_root);

protected:
    static void _bind_methods();

public:
    USDPlugin();
    ~USDPlugin();

    virtual void _enter_tree() override;
    virtual void _exit_tree() override;
    virtual bool _has_main_screen() const override;
    virtual String _get_plugin_name() const override;
    
    void _on_hello_button_pressed();
};

}

#endif // USD_PLUGIN_H
