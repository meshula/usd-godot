#ifndef USD_EXPORT_SETTINGS_H
#define USD_EXPORT_SETTINGS_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class UsdDocument;

class UsdExportSettings : public Resource {
    GDCLASS(UsdExportSettings, Resource);

private:
    // Export options
    bool _export_materials;
    bool _export_animations;
    bool _export_cameras;
    bool _export_lights;
    bool _export_meshes;
    bool _export_textures;
    String _copyright;
    float _bake_fps;
    
    // USD-specific options
    bool _use_binary_format; // .usdc vs .usda
    bool _flatten_stage;
    bool _export_as_single_layer;
    bool _export_with_references;

protected:
    static void _bind_methods();

public:
    UsdExportSettings();
    
    // Getters and setters
    void set_export_materials(bool p_enabled);
    bool get_export_materials() const;
    
    void set_export_animations(bool p_enabled);
    bool get_export_animations() const;
    
    void set_export_cameras(bool p_enabled);
    bool get_export_cameras() const;
    
    void set_export_lights(bool p_enabled);
    bool get_export_lights() const;
    
    void set_export_meshes(bool p_enabled);
    bool get_export_meshes() const;
    
    void set_export_textures(bool p_enabled);
    bool get_export_textures() const;
    
    void set_copyright(const String &p_copyright);
    String get_copyright() const;
    
    void set_bake_fps(float p_fps);
    float get_bake_fps() const;
    
    void set_use_binary_format(bool p_enabled);
    bool get_use_binary_format() const;
    
    void set_flatten_stage(bool p_enabled);
    bool get_flatten_stage() const;
    
    void set_export_as_single_layer(bool p_enabled);
    bool get_export_as_single_layer() const;
    
    void set_export_with_references(bool p_enabled);
    bool get_export_with_references() const;
    
    // Generate property list for the inspector
    void generate_property_list(Ref<UsdDocument> p_document, Node *p_root = nullptr);
};

}

#endif // USD_EXPORT_SETTINGS_H
