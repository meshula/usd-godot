#ifndef USD_DOCUMENT_H
#define USD_DOCUMENT_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/error_macros.hpp>

namespace godot {

class UsdState;

class UsdDocument : public Resource {
    GDCLASS(UsdDocument, Resource);

private:
    // USD-specific members
    // These will depend on the USD API and will be expanded as we implement the USD integration
    
    // Helper methods
    void _traverse_scene(Node *p_node, int p_depth);

protected:
    static void _bind_methods();

public:
    UsdDocument();
    
    // Export methods
    Error append_from_scene(Node *p_scene_root, Ref<UsdState> p_state, int32_t p_flags = 0);
    Error write_to_filesystem(Ref<UsdState> p_state, const String &p_path);
    
    // Helper methods
    String get_file_extension_for_format(bool p_binary) const;
};

}

#endif // USD_DOCUMENT_H
