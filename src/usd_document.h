#ifndef USD_DOCUMENT_H
#define USD_DOCUMENT_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/string.hpp>

// USD headers
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/sdf/path.h>

namespace godot {

class UsdState;

class UsdDocument : public Resource {
    GDCLASS(UsdDocument, Resource);

protected:
    static void _bind_methods();

public:
    UsdDocument();

    // Export methods
    Error append_from_scene(Node *p_scene_root, Ref<UsdState> p_state, int32_t p_flags = 0);
    Error write_to_filesystem(Ref<UsdState> p_state, const String &p_path);
    String get_file_extension_for_format(bool p_binary) const;

    // Import methods
    Error import_from_file(const String &p_path, Node *p_parent, Ref<UsdState> p_state);

private:
    // Export helpers
    void _convert_node_to_prim(Node *p_node, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath &p_parent_path, Ref<UsdState> p_state);

    // Import helpers
    Error _import_prim_hierarchy(const pxr::UsdStageRefPtr &p_stage, const pxr::SdfPath &p_prim_path, Node *p_parent, Ref<UsdState> p_state);
};

} // namespace godot

#endif // USD_DOCUMENT_H
